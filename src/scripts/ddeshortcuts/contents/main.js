if (workspace) {
    registerShortcut("Window Absolute Maximize", "Window Absolute Maximize", "Meta+Up", function() {
        workspace.maximizeActiveClient();
    })
    registerShortcut("Window Unmaximize", "Window Unmaximize", "Meta+Down", function() {
        workspace.restoreActiveClient();
    })
}

