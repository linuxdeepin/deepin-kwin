#include "systemclient.h"
#include <qapplication.h>
#include <qcursor.h>
#include <qabstractlayout.h>
#include <qlayout.h>
#include <qtoolbutton.h>
#include <qlabel.h>
#include <qdrawutil.h>
#include <kpixmapeffect.h>
#include <qbitmap.h>
#include "workspace.h"
#include "options.h"

static unsigned char iconify_bits[] = {
    0x00, 0x00, 0xff, 0xff, 0x7e, 0x3c, 0x18, 0x00 };
 
static unsigned char close_bits[] = {
    0xc3, 0x66, 0x3c, 0x18, 0x3c, 0x66, 0xc3, 0x00 };
 
static unsigned char maximize_bits[] = {
    0x3f, 0x9f, 0xcf, 0x67, 0x33, 0x19, 0x0c, 0x06 };

static QPixmap *titlePix=0;
static KPixmap *aFramePix=0;
static KPixmap *iFramePix=0;
static KPixmap *aHandlePix=0;
static KPixmap *iHandlePix=0;
static bool pixmaps_created = false;

static void create_pixmaps()
{
    if(pixmaps_created)
        return;
    pixmaps_created = true;

    // titlebar
    QPainter p;
    int i, y;
    titlePix = new QPixmap(32, 18);
    p.begin(titlePix);
    p.fillRect(0, 0, 32, 18,
               QBrush(options->color(Options::Frame, true),
                      QBrush::SolidPattern));
    for(i=0, y=5; i < 4; ++i, y+=3){
        p.setPen(options->color(Options::TitleBar, true).light(150));
        p.drawLine(0, y, 31, y);
        p.setPen(options->color(Options::TitleBar, true).dark(120));
        p.drawLine(0, y+1, 31, y+1);
    }
    p.end();

    // Bottom frame gradient
    aFramePix = new KPixmap();
    aFramePix->resize(32, 6);
    KPixmapEffect::gradient(*aFramePix,
                            options->color(Options::Frame, true).light(150),
                            options->color(Options::Frame, true).dark(120),
                            KPixmapEffect::VerticalGradient);
    iFramePix = new KPixmap();
    iFramePix->resize(32, 6);
    KPixmapEffect::gradient(*iFramePix,
                            options->color(Options::Frame, false).light(150),
                            options->color(Options::Frame, false).dark(120),
                            KPixmapEffect::VerticalGradient);

    // Handle gradient
    aHandlePix = new KPixmap();
    aHandlePix->resize(32, 6);
    KPixmapEffect::gradient(*aHandlePix,
                            options->color(Options::Handle, true).light(150),
                            options->color(Options::Handle, true).dark(120),
                            KPixmapEffect::VerticalGradient);
    iHandlePix = new KPixmap();
    iHandlePix->resize(32, 6);
    KPixmapEffect::gradient(*iHandlePix,
                            options->color(Options::Handle, false).light(150),
                            options->color(Options::Handle, false).dark(120),
                            KPixmapEffect::VerticalGradient);

}

SystemButton::SystemButton(const unsigned char *bitmap, QWidget *parent,
                           const char *name)
    : QButton(parent, name)
{
    QPainter p;

    aBackground.resize(16, 16);
    iBackground.resize(16, 16);

    QColor hColor(options->color(Options::ButtonBg, true));
    QColor lColor(options->color(Options::ButtonBlend, true));
    // only do this if we can dim/brighten equally
    if(hColor.red() < 226 && hColor.green() < 226 && hColor.blue() < 226)
        hColor.setRgb(hColor.red()+30, hColor.green()+30, hColor.blue()+30);
    if(lColor.red() > 29 && lColor.green() > 29 && lColor.blue() > 29)
        lColor.setRgb(lColor.red()-30, lColor.green()-30, lColor.blue()-30);
    KPixmapEffect::gradient(iBackground, hColor, lColor,
                            KPixmapEffect::DiagonalGradient);

    hColor = options->color(Options::ButtonBlend, false);
    lColor = options->color(Options::ButtonBg, false);
    if(hColor.red() > 29 && hColor.green() > 29 && hColor.blue() > 29)
        hColor.setRgb(hColor.red()-30, hColor.green()-30, hColor.blue()-30);
    if(lColor.red() < 226 && lColor.green() < 226 && lColor.blue() < 226)
        lColor.setRgb(lColor.red()+30, lColor.green()+30, lColor.blue()+30);
    KPixmapEffect::gradient(aBackground, hColor, lColor,
                            KPixmapEffect::DiagonalGradient);

    KPixmap aInternal;
    aInternal.resize(10, 10);
    KPixmap iInternal;
    iInternal.resize(10, 10);
    KPixmapEffect::gradient(iInternal,
                            options->color(Options::ButtonBlend, true),
                            options->color(Options::ButtonBg, true),
                            KPixmapEffect::DiagonalGradient);
    KPixmapEffect::gradient(aInternal,
                            options->color(Options::ButtonBg, false),
                            options->color(Options::ButtonBlend, false),
                            KPixmapEffect::DiagonalGradient);

    p.begin(&iBackground);
    p.drawPixmap(3, 3, iInternal);
    p.setPen(Qt::black);
    p.drawRect(0, 0, 16, 16);
    p.end();

    p.begin(&aBackground);
    p.drawPixmap(3, 3, aInternal);
    p.setPen(Qt::black);
    p.drawRect(0, 0, 16, 16);
    p.end();
    
    deco = QBitmap(8, 8, bitmap);
    deco.setMask(deco);
    resize(16, 16);
    
    QBitmap mask;
    mask.resize(16, 16);
    mask.fill(color1);
    p.begin(&mask);
    p.setPen(color0);
    p.drawPoint(0, 0);
    p.drawPoint(15, 0);
    p.drawPoint(0, 15);
    p.drawPoint(15, 15);
    p.end();
    setMask(mask);
    
}

void SystemButton::drawButton(QPainter *p)
{
    if(isDown())
        p->drawPixmap(0, 0, aBackground);
    else
        p->drawPixmap(0, 0, iBackground);

    p->setPen(options->color(Options::ButtonFg, isDown()));
    p->drawPixmap(isDown() ? 5 : 4, isDown() ? 5 : 4, deco);
}
    
SystemClient::SystemClient( Workspace *ws, WId w, QWidget *parent,
                            const char *name )
    : Client( ws, w, parent, name, WResizeNoErase )
{
    create_pixmaps();

    QGridLayout* g = new QGridLayout(this, 0, 0, 2);
    g->setRowStretch(1, 10);
    g->addWidget(windowWrapper(), 1, 1 );
    g->addRowSpacing(2, 6);

    button[0] = new SystemButton(close_bits, this );
    button[1] = new SystemButton(iconify_bits, this );
    button[2] = new SystemButton(maximize_bits, this );
    connect( button[0], SIGNAL( clicked() ), this, ( SLOT( closeWindow() ) ) );
    connect( button[1], SIGNAL( clicked() ), this, ( SLOT( iconify() ) ) );
    connect( button[2], SIGNAL( clicked() ), this, ( SLOT( maximize() ) ) );

    QHBoxLayout* hb = new QHBoxLayout();
    g->addLayout( hb, 0, 1 );
    hb->addSpacing(2);
    hb->addWidget( button[0] );
    titlebar = new QSpacerItem(10, 18, QSizePolicy::Expanding,
                               QSizePolicy::Minimum );
    hb->addItem(titlebar);
    hb->addSpacing(2);
    hb->addWidget( button[1] );
    hb->addWidget( button[2] );
    hb->addSpacing(2);

    for ( int i = 0; i < 3; i++) {
        button[i]->setMouseTracking( TRUE );
        button[i]->setFixedSize( 16, 16 );
    }


}

void SystemClient::resizeEvent( QResizeEvent* e)
{
    Client::resizeEvent( e );

    if ( isVisibleToTLW() ) {
        QPainter p( this );
	QRect t = titlebar->geometry();
	t.setTop( 0 );
	QRegion r = rect();
	r = r.subtract( t );
	p.setClipRegion( r );
	p.eraseRect( rect() );
    }
}

void SystemClient::captionChange( const QString& )
{
    repaint( titlebar->geometry(), false );
}


void SystemClient::paintEvent( QPaintEvent* )
{
    QPainter p( this );
    p.setPen(Qt::black);
    p.drawRect(rect());
    QRect t = titlebar->geometry();
    t.setTop( 1 );
    if(isActive())
        p.drawTiledPixmap(t, *titlePix);
    else
        p.fillRect(t, options->colorGroup(Options::Frame, false).
                   brush(QColorGroup::Button));

    qDrawShadePanel(&p, rect().x()+1, rect().y()+1, rect().width()-2,
                    rect().height()-2,
                    options->colorGroup(Options::Frame, isActive()), false);


    QRegion r = rect();
    r = r.subtract( t );
    p.setClipRegion( r );
    p.setClipping( FALSE );

    t.setTop( 2 );
    t.setLeft( t.left() + 4 );
    t.setRight( t.right() - 2 );

    p.setPen(options->color(Options::Font, isActive()));
    p.setFont(options->font(isActive()));
    p.setBackgroundMode(OpaqueMode);
    p.drawText( t, AlignCenter, caption() );
    p.setBackgroundMode(TransparentMode);

    qDrawShadePanel(&p, rect().x()+1, rect().bottom()-6, 24, 6,
                    options->colorGroup(Options::Handle, isActive()), false);
    p.drawTiledPixmap(rect().x()+2, rect().bottom()-5, 22, 4,
                      isActive() ? *aHandlePix : *iHandlePix);

    qDrawShadePanel(&p, rect().x()+25, rect().bottom()-6, rect().width()-50, 6,
                    options->colorGroup(Options::Frame, isActive()), false);
    p.drawTiledPixmap(rect().x()+26, rect().bottom()-5, rect().width()-52, 4,
                      isActive() ? *aFramePix : *iFramePix);

    qDrawShadePanel(&p, rect().right()-24, rect().bottom()-6, 24, 6,
                    options->colorGroup(Options::Handle, isActive()), false);
    p.drawTiledPixmap(rect().right()-23, rect().bottom()-5, 22, 4,
                      isActive() ? *aHandlePix : *iHandlePix);
}

void SystemClient::mouseDoubleClickEvent( QMouseEvent * e )
{
    if (titlebar->geometry().contains( e->pos() ) )
        setShade( !isShade() );
    workspace()->requestFocus( this );
}


void SystemClient::init()
{
    //
}

