/*
 * Reproduce Q_ASSERT(m_forceVisibleByDesktopCount > 0) in MultitaskViewEffect.
 *
 * Prerequisites:
 *   - Running on Wayland session with XWayland support
 *   - gcc, libxcb-dev, dbus installed
 *
 * Build:
 *   gcc -o test_mtview_assert test_mtview_assert.c -lxcb
 *
 * DISPLAY sniffing:
 *   The program does NOT rely on $DISPLAY alone. It probes, in order:
 *     1. $DISPLAY env var  (validated by a real xcb connection)
 *     2. /proc/PID/cmdline (Xwayland/Xorg/Xephyr argv ":N" token)
 *     3. /tmp/.X11-unix/   (socket files "X<N>", lowest N wins)
 *     4. linear probe       (":0" .. ":31")
 *   The first candidate that yields a working xcb connection wins, so the
 *   reproduction works even when $DISPLAY is unset/wrong under a Wayland
 *   session whose XWayland runs on an unexpected display number.
 *
 * Usage:
 *   ./test_mtview_assert
 *   The program activates multitask view, creates XWayland windows to set
 *   up the race, and lets the view auto-deactivate (flying-back animation)
 *   so cleanup() runs and hits the assertion.
 *
 * Mechanism:
 *   - Window V: mapped via XCB but no drawing operations performed.
 *     In KWin's X11/Xwayland code path, addX11Window() calls addToStack()
 *     before readyForPainting is set (depends on XSYNC/damage events).
 *     So V enters stacking_order, but effects->windowAdded for V is
 *     deferred (waiting for Window::windowShown).
 *
 *   - Window W: mapped via XCB and immediately drawn (poly_fill_rectangle).
 *     Drawing triggers damage -> XWayland bridge commits buffer ->
 *     KWin sets readyForPainting -> windowShown -> effects->windowAdded ->
 *     MultitaskViewEffect::onWindowAdded(W). On the X11 platform W is not a
 *     dock/lock/recorder, so the handler calls m_effectFlyingBack.begin()
 *     and addRepaintFull(). The flying-back animation runs and, once done,
 *     postPaintScreen() calls setActive(false) -> cleanup().
 *
 *   - cleanup() iterates effects->stackingOrder() (pre-fix behavior),
 *     which includes V. V was never ref'd by MultitaskViewEffect (its
 *     effects->windowAdded hasn't fired yet), so
 *     m_forceVisibleByDesktopCount == 0. unrefVisible(DESKTOP) hits the
 *     Q_ASSERT.
 *
 *   With the fix applied, cleanup() only unrefs windows in
 *   m_windowVisibleRefs (a QHash<EffectWindow*, EffectWindowVisibleRef>),
 *   so V — which was never inserted into the hash — is skipped and the
 *   assert does NOT fire. Run this test against fixed KWin to confirm no
 *   crash; revert the fix and the assert reappears.
 *
 * The race window is narrow. Run multiple times if needed (-r option).
 */

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <xcb/xcb.h>

/* ---- DISPLAY sniffing ------------------------------------------------- */

/*
 * Return malloc'd ":N" for the first working X display found, or NULL.
 * Caller frees. Each candidate is validated by opening a real xcb
 * connection and checking xcb_connection_has_error().
 */
static int validate_display(const char *dpy)
{
    xcb_connection_t *c = xcb_connect(dpy, NULL);
    int ok = c && !xcb_connection_has_error(c);
    if (c)
        xcb_disconnect(c);
    return ok;
}

/* Parse ":N" or ":N.M"; return N in [0,99] or -1. */
static int parse_display_num(const char *s)
{
    if (!s || s[0] != ':')
        return -1;
    char *end;
    long n = strtol(s + 1, &end, 10);
    if (end == s + 1)
        return -1;
    if (*end != '\0' && *end != '.')
        return -1;
    if (n < 0 || n > 99)
        return -1;
    return (int)n;
}

/* Priority 2: scan /proc for an X server process, grab its ":N" argv. */
static char *sniff_from_xserver_proc(void)
{
    DIR *dir = opendir("/proc");
    if (!dir)
        return NULL;

    char *result = NULL;
    struct dirent *e;
    while ((e = readdir(dir)) != NULL) {
        const char *n = e->d_name;
        if (*n < '0' || *n > '9')
            continue; /* only PID dirs */

        char path[64];
        snprintf(path, sizeof(path), "/proc/%s/cmdline", n);
        int fd = open(path, O_RDONLY);
        if (fd < 0)
            continue;
        char buf[1024];
        ssize_t len = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (len <= 0)
            continue;
        buf[len] = '\0';

        /* First NUL-separated token is argv0; compare its basename. */
        const char *base = strrchr(buf, '/');
        base = base ? base + 1 : buf;
        if (strcmp(base, "Xwayland") != 0 && strcmp(base, "Xorg") != 0
            && strcmp(base, "Xephyr") != 0)
            continue;

        /* Walk argv tokens looking for a ":N" display spec. */
        const char *p = buf;
        const char *endp = buf + len;
        while (p < endp) {
            int num = parse_display_num(p);
            if (num >= 0) {
                char *out = malloc(8);
                if (out)
                    snprintf(out, 8, ":%d", num);
                result = out;
                goto done;
            }
            p += strlen(p) + 1;
        }
    }
done:
    closedir(dir);
    return result;
}

/* Priority 3: scan /tmp/.X11-unix for "X<N>" sockets, lowest N wins. */
static char *sniff_from_x11_sockets(void)
{
    DIR *dir = opendir("/tmp/.X11-unix");
    if (!dir)
        return NULL;

    int best = -1;
    struct dirent *e;
    while ((e = readdir(dir)) != NULL) {
        if (e->d_name[0] != 'X')
            continue;
        char *end;
        long v = strtol(e->d_name + 1, &end, 10);
        if (end == e->d_name + 1 || *end != '\0' || v < 0 || v > 99)
            continue;
        if (best < 0 || v < best)
            best = (int)v;
    }
    closedir(dir);
    if (best < 0)
        return NULL;
    char *out = malloc(8);
    if (out)
        snprintf(out, 8, ":%d", best);
    return out;
}

/* Priority 4: linear probe ":0" .. ":31". */
static char *sniff_by_probe(void)
{
    char buf[8];
    for (int n = 0; n <= 31; n++) {
        snprintf(buf, sizeof(buf), ":%d", n);
        if (validate_display(buf))
            return strdup(buf);
    }
    return NULL;
}

static char *sniff_display(void)
{
    /* 1. $DISPLAY env var. */
    const char *env = getenv("DISPLAY");
    if (env && *env && validate_display(env))
        return strdup(env);

    /* 2. Xwayland/Xorg/Xephyr process cmdline. */
    char *p = sniff_from_xserver_proc();
    if (p) {
        if (validate_display(p))
            return p;
        free(p);
    }

    /* 3. /tmp/.X11-unix sockets. */
    p = sniff_from_x11_sockets();
    if (p) {
        if (validate_display(p))
            return p;
        free(p);
    }

    /* 4. linear probe. */
    return sniff_by_probe();
}

/* ---- end DISPLAY sniffing --------------------------------------------- */

static xcb_window_t create_window(xcb_connection_t *conn, xcb_screen_t *screen,
                                  int x, int draw)
{
    xcb_window_t win = xcb_generate_id(conn);

    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t values[2] = {
        screen->white_pixel,
        XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY};

    xcb_create_window(conn, XCB_COPY_FROM_PARENT, win, screen->root,
                      x, 0, 200, 200, 1,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      screen->root_visual, mask, values);
    xcb_map_window(conn, win);
    xcb_flush(conn);

    if (draw) {
        xcb_gcontext_t gc = xcb_generate_id(conn);
        uint32_t gc_mask = XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
        uint32_t gc_values[2] = {screen->black_pixel, 0};
        xcb_create_gc(conn, gc, win, gc_mask, gc_values);
        xcb_poly_fill_rectangle(conn, win, gc, 1,
                                (xcb_rectangle_t[]){{0, 0, 200, 200}});
        xcb_flush(conn);
    }

    return win;
}

/*
 * Activate multitask view by invoking the ShowMultitasking global shortcut
 * via org.kde.kglobalaccel DBus interface.
 */
static void activate_multitask_view(void)
{
    int ret = system("gdbus call --session --dest org.kde.kglobalaccel "
                     "--object-path /component/kwin "
                     "--method org.kde.kglobalaccel.Component.invokeShortcut "
                     "'ShowMultitasking' >/dev/null 2>&1");
    if (ret != 0) {
        fprintf(stderr, "[warn] Failed to activate multitask view\n");
    }
}

static int run_once(const char *dpy)
{
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        perror("pipe");
        return 1;
    }

    /* Activate multitask view before creating any windows */
    printf("[main] Activating multitask view...\n");
    activate_multitask_view();

    /* Give KWin time to enter the active state */
    usleep(200000); /* 200ms */

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        /* Child: create window V without drawing */
        close(pipefd[0]);

        xcb_connection_t *conn = xcb_connect(dpy, NULL);
        if (xcb_connection_has_error(conn)) {
            fprintf(stderr, "[child] Cannot connect to X display (%s)\n",
                    dpy ? dpy : "<default>");
            close(pipefd[1]);
            _exit(1);
        }

        xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

        (void)create_window(conn, screen, 0, 0);
        printf("[child] Window V created and mapped (no drawing)\n");

        /* Signal parent that V has been mapped */
        char buf = 1;
        write(pipefd[1], &buf, 1);
        close(pipefd[1]);

        /* Keep V alive */
        sleep(5);
        xcb_disconnect(conn);
        _exit(0);
    }

    /* Parent: wait for child to map V, then create W with drawing */
    close(pipefd[1]);

    char buf;
    read(pipefd[0], &buf, 1);
    close(pipefd[0]);

    /* Brief delay to let KWin process V's map request */
    usleep(50000); /* 50ms */

    xcb_connection_t *conn = xcb_connect(dpy, NULL);
    if (xcb_connection_has_error(conn)) {
        fprintf(stderr, "[parent] Cannot connect to X display (%s)\n",
                dpy ? dpy : "<default>");
        wait(NULL);
        return 1;
    }

    xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

    printf("[parent] Creating window W (with drawing)...\n");
    (void)create_window(conn, screen, 300, 1);

    /* W's onWindowAdded begins the flying-back animation, which on done
     * runs setActive(false) -> cleanup() while V is still in
     * stacking_order but never ref'd. Just wait for it. */
    printf("[parent] Waiting for flying-back animation to deactivate the "
           "view...\n");
    printf("[parent] With the fix: no assertion (regression test passes).\n");
    printf("[parent] Without the fix: check kwin_wayland output for the "
           "Q_ASSERT failure.\n\n");

    sleep(5);
    xcb_disconnect(conn);

    int status;
    waitpid(pid, &status, 0);
    return 0;
}

static void usage(const char *prog)
{
    printf("Usage: %s [-r N]\n", prog);
    printf("  -r N    Repeat N times (default: 1)\n");
}

int main(int argc, char *argv[])
{
    int repeats = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            repeats = atoi(argv[++i]);
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    if (repeats <= 0) {
        repeats = 1;
    }

    printf("=== MultitaskView Assert Reproduction ===\n\n");

    char *dpy = sniff_display();
    if (!dpy) {
        fprintf(stderr, "[main] No usable X display found "
                        "(env /proc /tmp/.X11-unix / probe all failed).\n");
        return 1;
    }
    printf("[main] Using X display: %s\n\n", dpy);

    for (int i = 0; i < repeats; i++) {
        printf("--- Attempt %d/%d ---\n", i + 1, repeats);
        run_once(dpy);
        printf("\n");
    }

    free(dpy);
    return 0;
}
