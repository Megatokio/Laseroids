/*	Copyright  (c)	Günter Woigk 2016 - 2019
					mailto:kio@little-bat.de

	This file is free software.

	Permission to use, copy, modify, distribute, and sell this software
	and its documentation for any purpose is hereby granted without fee,
	provided that the above copyright notice appears in all copies and
	that both that copyright notice, this permission notice and the
	following disclaimer appear in supporting documentation.

	THIS SOFTWARE IS PROVIDED "AS IS", WITHOUT ANY WARRANTY, NOT EVEN THE
	IMPLIED WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
	AND IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY DAMAGES
	ARISING FROM THE USE OF THIS SOFTWARE,
	TO THE EXTENT PERMITTED BY APPLICABLE LAW.
*/

#include "kio/kio.h"
#include "VectorDisplay.h"
#include <QWidget>
#include <QPainter>
#include <QPixmap>
#include <math.h>
#include <QResizeEvent>
#include <QTimer>
#include <QGLWidget>
#include <QGLFormat>

#define L	int8(254)
#define E	int8(255)
#include "VectorDisplayFont.h"		// int8 vt_font_data[];

static uint vt_font_col1[256];		// index in vt_font_data[]
static int8 vt_font_width[256];		// character print width (including +1 for line width but no spacing)

ON_INIT([]()
{
	for (uint i = 0, c=' '; c<NELEM(vt_font_col1) && i<NELEM(vt_font_data); c++)
	{
		xxlogline(charstr(c));
		vt_font_col1[c] = i;
		i += 2;						// left-side mask and right-side mask

		int8 width = 0;
		while (vt_font_data[i++] == L)
		{
			while (vt_font_data[i]!=L && vt_font_data[i]!=E)
			{
				width = max(width,vt_font_data[i]);
				i += 2;
			}
		}
		vt_font_width[c] = width + 1;

		assert(vt_font_data[i-1] == E);
	}
});
#undef L
#undef E

int printWidth (const uchar* s)
{
	// calculate print width for string
	// as printed by drawing command DrawText
	// for text size = 8
	// and text width 1.0

	int width = 0;
	uint8 mask = 0;

	while (uint8 c = uint8(*s++))
	{
		width += vt_font_width[c];
		int8* p = vt_font_data + vt_font_col1[c];
		if (mask & uint8(*p)) width++;	// +1 if glyphs would touch
		mask = uint8(*(p+1));			// remember for next
	}
	return width;
}

inline int printWidth (const char* s)
{
	return printWidth(reinterpret_cast<const uchar*>(s));
}


/*	Notes:

  Base Class QWidget:

	Ein richtiges, transparentes Fenster bekommt man nur mit BaseClass QWidget.
	macOS: Der Fensterschatten lässt sich NICHT mit setAttribute(Qt::WA_MacNoShadow,true) ausschalten.
	Der Fensterschatten wird gemäß dem Fensterinhalt beim ersten Anzeigen bzw. bei Wechsel von
	Selected/Deselected (nur wenn nicht FramelessWindowHint) berechnet und danach "eingefroren".
	Hier wird man manuell einen ersten Screen anzeigen müssen, der komplett leer ist.

	Die Zeichenoberfläche wird vor jedem paintEvent() gelöscht (transparent/schwarz).
	Ein Nachleuchteffekt ist mit QWidget nicht möglich.

	Eine Synchronisierung mit dem FFB ist für QWidget nicht möglich.

	Die Prozessorlast ist deutlich höher.

  Base Class QGLWidget

	Die gezeichneten Linien sind schlechter geglättet als beim QWidget.

	Ein transparentes Fenster lässt sich nicht darstellen.
	TODO: eventuell die Zeichenoberfläche vorab alpha=0 löschen.

	Die Zeichenoberfläche wird nicht vorab gelöscht und enthält den alten Inhalt
	(bei Double-Buffering den des vor-dem-alten) der abgedimmt werden kann.
	Ein Nachleuchteffekt ist mit QGlWidget in einem opaquen Fenster einfach möglich.

	Eine Synchronisierung mit dem FFB ist möglich:
		QGLFormat.setSwapInterval(1) und QTimer.setInterval(0)
*/

const VectorDisplay::ColorSet VectorDisplay::PAPERWHITE{
	QColor(0xF0, 0xF0, 0xF0, 0xFF), 	// paper, no fading
	QColor(0x00, 0x00, 0x30, 0xC0)};	// lines
const VectorDisplay::ColorSet VectorDisplay::WHITE{
	QColor(0xE0/8,0xE0/8,0xE0/8, 0x80),	// paper, fading
	QColor(0xE0/1,0xE0/1,0xE0/1, 0x80)};// lines
const VectorDisplay::ColorSet VectorDisplay::AMBER{
	QColor(0xed/4,0xb8/4,0x09/4, 0x40),
	QColor(0xed/1,0xb8/1,0x09/1, 0x80)};
const VectorDisplay::ColorSet VectorDisplay::BLUE{
	QColor(0x63/8,0x9f/8,0xff/8, 0x60),
	QColor(0x63/1,0x9f/1,0xff/1, 0x80)};
const VectorDisplay::ColorSet VectorDisplay::GREEN{
	QColor(0xCC/6,0xFF/6,0x22/6, 0x60),
	QColor(0xCC/1,0xFF/1,0x22/1, 0x80)};


void VectorDisplay::setBackgroundColor (int r, int g, int b, int a) noexcept
{
	// alpha for luminescent fading

	display.background_color = QColor(r,g,b);
	display.blur_color = QColor(r,g,b,a);

	if ((0))	// somehow no visible effect ... ?!?
	{
		int d = a >= 0xff ? 0 : a >= 0x7F ? 1 : a >= 0x3F ? 3 : 7;
		if (r+g+b < 0x180) display.blur_color = QColor(max(0,r-d),max(0,g-d),max(0,b-d),a);
		else			   display.blur_color = QColor(min(255,r+d),min(255,g+d),min(255,b+d),a);
	}

	//setStyleSheet("background-color:black");
	if (this->isVisible())
		QPainter(this).fillRect(this->rect(),display.background_color);
	//update();
}

void VectorDisplay::setBackgroundColor (QColor background) noexcept
{
	int r,g,b,a;
	background.getRgb(&r,&g,&b,&a);
	setBackgroundColor(r,g,b,a);
}

void VectorDisplay::setDisplayColors (const ColorSet& cs) noexcept
{
	setBackgroundColor(cs.background);

	display.default_pen = QPen(cs.lines, 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
	display.default_brush = Qt::NoBrush;
}

uint VectorDisplay::setDisplayStyle (DisplayStyle style) noexcept
{
	if (USE_OGL) style &= ~TRANSPARENT; // OGLWidget can't transparent
	else		 style &= ~FADING;		// QWidget can't fading

	display.style = DisplayStyle(style);

	if(style & (TRANSPARENT | FADING))
	{
		setStyleSheet("background:transparent;");
		setAttribute(Qt::WA_TranslucentBackground);
	}
	else
	{
		// TODO();
	}

	if (style & FRAMELESS)
		 setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
	else setWindowFlags(windowFlags() & ~Qt::FramelessWindowHint);

	return style;	// actually applied style
}

bool VectorDisplay::setTransparent (bool f) noexcept
{
	return setDisplayStyle(f ? display.style|TRANSPARENT : display.style & ~TRANSPARENT) & TRANSPARENT;
}
bool VectorDisplay::setFrameless (bool f) noexcept
{
	return setDisplayStyle(f ? display.style|FRAMELESS : display.style & ~FRAMELESS) & FRAMELESS;
}
bool VectorDisplay::setFading (bool f) noexcept
{
	return setDisplayStyle(f ? display.style|FADING : display.style & ~FADING) & FADING;
}


#if USE_OGL
QGLFormat myQGLformat()
{
	QGLFormat fmt;
	fmt.setSwapInterval(1);		// -> QTimer.setInterval(0) --> sync to FFB
	//fmt.setDoubleBuffer(true);// true=default. required for swap interval (sync. with ffb)
	return fmt;
}
#endif


VectorDisplay::VectorDisplay (QWidget* parent, QSize size, const ColorSet& cs, DisplayStyle ds) :
#if USE_OGL
	QGLWidget{myQGLformat(),parent}
#else
	QWidget{parent}
#endif
{
	resize(size);							// widget size
	display.size = QSize(1000,800);			// logical size (default)
	setDisplayColors(cs);					// default colors & background
	setDisplayStyle(ds);

#if USE_OGL
	display.synchronizes_with_vblank = format().swapInterval() == 1; // strictly != -1, but we set swapInterval in myOGLformat.
	if (display.synchronizes_with_vblank)
		qDebug("Swap Buffers at v_blank AVAILABLE :-)");
	else
		qDebug("Swap Buffers at v_blank NOT AVAILABLE: refreshing at ~60fps.");
#endif

	//QWidget::setEnabled(true);			// enable mouse & kbd events (true=default)
	setFocusPolicy(Qt::StrongFocus);		// sonst kriegt das Toolwindow manchmal keine KeyEvents

	//setAttribute(Qt::WA_OpaquePaintEvent,true);	// doesn't work: wir malen alle Pixel. Qt muss nicht vorher löschen.
	//setAttribute(Qt::WA_NoSystemBackground,true);	// ?
	//setAttribute(Qt::WA_MacNoShadow,true);		// doesn't work
	//setAttribute(Qt::WA_TintedBackground,false);	// ?
}

VectorDisplay::~VectorDisplay()
{}

void VectorDisplay::focusInEvent (QFocusEvent* e)
{
	//update();
	QWidget::focusInEvent(e);
	emit focusChanged(1);
}

void VectorDisplay::focusOutEvent (QFocusEvent* e)
{
	//update();
	QWidget::focusOutEvent(e);
	emit focusChanged(0);
}

void VectorDisplay::resizeEvent (QResizeEvent* e)
{
	QWidget::resizeEvent(e);
	//QPainter(this).fillRect(this->rect(),display.background);
	//update();
}

void VectorDisplay::showEvent (QShowEvent* e)
{
	QWidget::showEvent(e);
	//QPainter(this).fillRect(this->rect(),display.background);
	//update();
}

void VectorDisplay::paintEvent (QPaintEvent*)
{
	if (1) // statistics
	{
		static uint8 n = 0;
		static double stime = now();
		static double mw = 0.020;
		double d = now()-stime;
		stime += d;
		mw = mw*0.99 + d*0.01;
		if(++n==0) qDebug("d = %.2f ms, mw = %.2f ms", d*1000, mw*1000);
	}

	// get render data:
	pipeline_lock.lock();
	Array<qreal> drawing_data(std::move(new_drawing_data));
	Array<uint8> drawing_commands(std::move(new_drawing_commands));
	pipeline_lock.unlock();

	QPainter painter(this);

	if (display.style & FADING)
	{
		painter.fillRect(this->rect(), display.blur_color);	// fade
		if (drawing_commands.count() == 0) return;
	}
	else
	{
		if (drawing_commands.count() == 0) return;
		if (!(display.style & TRANSPARENT))
			painter.fillRect(this->rect(),display.background_color);
	}

	// transparent & USE_OGL:
	//	painter.setCompositionMode(QPainter::CompositionMode_Source);		// this only clears
	//	painter.fillRect(this->rect(),Qt::transparent);						// to all-black (non-transparent)


	// INFO:
	//	QTransform(m11, m12, m13, m21, m22, m23, m31, m32, m33 = 1.0)
	//	QTransform(m11, m12,      m21, m22,      dx,  dy)
	//	QTransform(a,   c,        b,   d,        dx,  dy)
	//
	//	The coordinates are transformed using the following formulas:
	//
	//	x' = m11*x + m21*y + dx
	//	y' = m22*y + m12*x + dy
	//	if (is not affine)
	//	{
	//		w' = m13*x + m23*y + m33
	//		x' /= w'
	//		y' /= w'
	//	}

	// scale CRT display to widget size
	// and flip vertically (0,0 is bottom left corner)
	// and apply distortion

	qreal fx = min(qreal(width())/display.size.width(), qreal(height())/display.size.height());
	qreal dx = width()/2  +fx/2;
	qreal dy = height()/2 -fx/2;
	qreal a = +this->a*fx, b = -this->b*fx, c = +this->c*fx, d = -this->d*fx;	// +a +c -b -d
	QTransform my_identity{a,c,b,d,dx,dy};
	painter.setWorldTransform(my_identity);

	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(display.default_pen);
	painter.setBrush(display.default_brush);

	// +++ draw drawing pipeline +++

	static_assert (sizeof(QPointF) == 2 * sizeof(qreal),"");
	static_assert (sizeof(QRectF) == 4 * sizeof(qreal),"");
	union
	{	qreal*   real;
		QPointF* point;
		QRectF*  rect;
	} data;

	data.real = drawing_data.getData();

	uint8* cmd = drawing_commands.getData();
	uint8* end = cmd + drawing_commands.count();

	QPointF p1 {0,0};			// current drawing position
	qreal text_scale_x = 1.0;	// current horizontal text scaling
	qreal text_scale_y = 1.0;	// current vertical text scaling
	uint8 rmask = 0;			// last kerning mask (rightmost pixel column of last char printed)
	QPointF p1_text {0,0};

	while (cmd < end)
	{
		switch (*cmd++)
		{
		case SelectPen:		// select Pen for drawing
		{
			uint idx = *cmd++;
			assert(idx < pens.count());
			painter.setPen(pens[idx]);
			continue;
		}
		case SelectBrush:	// select Brush for filling
		{
			uint idx = *cmd++;
			assert(idx < brushes.count());
			painter.setBrush(brushes[idx]);
			continue;
		}
		case MoveTo:		// move drawing cursor to absolute position
		{
			p1 = *data.point++;	// {x,y}
			continue;
		}
		case DrawTo:		// draw line from current to new absolute position
		{
			QPointF p2 = *data.point++;	// {x,y}
			painter.drawLine(p1,p2);
			p1 = p2;
			continue;
		}
		case Move:			// move drawing cursor relative		OBSOLETE?
		{
			p1 += *data.point++;	// {x,y}
			continue;
		}
		case Draw:			// draw line from current to new relative position	OBSOLETE?
		{
			QPointF p0 = p1;
			p1 += *data.point++;			// {x,y}
			painter.drawLine(p0,p1);
			continue;
		}
		case DrawHor:		// draw line from current to new relative horizontal position	OBSOLETE?
		{
			QPointF p0 = p1;
			p1.setX(p1.x() + *data.real++);
			painter.drawLine(p0,p1);
			continue;
		}
		case DrawVer: 		// draw line from current to new relative vertical position		OBSOLETE?
		{
			QPointF p0 = p1;
			p1.setY(p1.y() + *data.real++);
			painter.drawLine(p0,p1);
			continue;
		}
		case DrawLine:		// draw line with absolute positions
		{
			QPointF* a = data.point++;
			QPointF* e = data.point++;	p1 = *e;
			painter.drawLine(*a,*e);
			continue;
		}
		case DrawPolyline:	// draw path of n-1 lines with absolute positions
		{
			int count = *cmd++;
			painter.drawPolyline(data.point,count);
			data.point += count;
			continue;		// todo: update p1?
		}
		case DrawLines:		// draw bunch of n lines with absolute positions
		{
			int count = *cmd++;
			painter.drawLines(data.point,count);
			data.point += count*2;
			continue;
		}
		case DrawRectangle:	// draw rectangle: QRect
		{
			painter.drawRect(*data.rect++);
			continue;
		}
		case DrawTriangle:	// draw triangle: 3 points with absolute positions		OBSOLETE?
		{
			painter.drawConvexPolygon(data.point,3);
			data.point += 3;
			continue;
		}
		case DrawConvexPolygon:	// draw convex polygon: n points with absolute positions
		{
			int count = *cmd++;
			painter.drawConvexPolygon(data.point,count);
			data.point += count;
			continue;
		}
		case DrawPolygon:	// draw any polygon: n points with absolute positions
		{
			int count = *cmd++;
			painter.drawPolygon(data.point,count);
			data.point += count;
			continue;
		}
		case DrawEllipse:	// draw ellipse, filled
		{
			QPointF center = *data.point++;
			qreal   rx     = *data.real++;
			qreal   ry     = *data.real++;
			painter.drawEllipse(center,rx,ry);
			continue;
		}
		case SetTextSize:	// set vertical text size and width ratio
		{
			text_scale_y = (qreal(*cmd++ +1)) / 8;					// vertical scaling; divided by native font size
			text_scale_x = (qreal(*cmd++ +1)) * text_scale_y / 32;	// horizontal scaling;
			rmask = 0;
			continue;
		}
		case DrawText:		// draw 0-delimited text, moving current drawing position right
		{
			qreal x0 = p1.x();		// current drawing position
			qreal y0 = p1.y();

			// clear kerning mask if p1 was moved since last print cmd:
			if (p1 != p1_text) rmask = 0x00;

			if (*cmd++)		// centered?
			{
				rmask = 0x00;
				x0 -= printWidth(cmd) * text_scale_x / 2;
			}

			for (uchar c = *cmd++; c != 0; c = *cmd++)
			{
				const uint  i = vt_font_col1[c];
				int8* p = vt_font_data + i;

				if (rmask & uint8(*p++)) x0 += text_scale_x;	// apply kerning
				rmask = uint8(*p++);							// remember for next

				#define L	int8(254)
				#define E	int8(255)

				while (*p++ == L)
				{
					qreal points[32];	// up to 16 points. note: actual max is 13 for char '@'
					qreal* pp = points;

					*pp++ = x0 + *p++ * text_scale_x;	// starting point
					*pp++ = y0 + *p++ * text_scale_y;

					while (*p != E && *p != L)
					{
						*pp++ = x0 + *p++ * text_scale_x; // next point
						*pp++ = y0 + *p++ * text_scale_y;
					}

					painter.drawPolyline(reinterpret_cast<QPointF*>(points), int(pp-points)/2);
				}

				assert(*(p-1) == E);

				#undef L
				#undef E

				x0 += vt_font_width[c] * text_scale_x;
			}

			p1.setX(x0);
			p1_text = p1;
			continue;
		}
		case PushTransformation:
		{
			TODO();
		}
		case PopTransformation:
		{
			TODO();
		}
		case SetTransformation:
		{
			// set HW transformation: scaling, rotation and distortion
			// values should not exceed -1 / +1
			// x = a*x + b*y
			// y = c*x + d*y

			qreal a = *data.real++;
			qreal b = *data.real++;
			qreal c = *data.real++;
			qreal d = *data.real++;
			painter.setWorldTransform(QTransform(a,c,b,d,0,0) * my_identity);
			continue;
		}
		case SetScaleAndRotation:
		{
			// set HW transformation: undistorted: scaled and rotated
			// scaling should be in range -1 … +1
			// rotation is ccw and should be in range -2PI … +2PI

			// a = d = f * cos(rad);
			// b =     f * sin(rad);
			// c = -b;

			qreal sin = *data.real++;
			qreal cos = *data.real++;
			painter.setWorldTransform(QTransform(cos,-sin,sin,cos,0,0) * my_identity);	// a c b d
			continue;
		}
		case SetScale:
		{
			// a = d = f;
			// b = c = 0;

			qreal f = *data.real++;
			painter.setWorldTransform(QTransform(f,0,0,f,0,0) * my_identity);	// a c b d
			continue;
		}
		case ResetTransformation:
		{
			painter.setWorldTransform(my_identity);	// a c b d
			continue;
		}
		default: IERR();
		}
	}

	// draw keyboard focus indicator:
	if (hasFocus())
	{
		painter.resetTransform();
		QPen pen(QColor(0x88,0xff,0x00,0x88));
		pen.setWidth(6);
		painter.setPen(pen);
		//painter.setCompositionMode(QPainter::CompositionMode_Darken);
		painter.drawRect(rect());
	}

	// move drawn data to old frame => reuse buffers and evtl. redraw old buffer in next event
	pipeline_lock.lock();
	old_drawing_commands = std::move(drawing_commands);
	old_drawing_data     = std::move(drawing_data);
	pipeline_lock.unlock();
}

void VectorDisplay::resetTransformation() noexcept
{
	drawing_commands << ResetTransformation;
}

void VectorDisplay::setTransformation (qreal a, qreal b, qreal c, qreal d) noexcept
{
	// set HW transformation: scaling, rotation and distortion
	// values should not exceed -1 / +1
	// x = a*x + b*y
	// y = c*x + d*y
	// Set to 1,0,0,1 --> display area: -128 … x … +127, -128 … y … +127

	// this->a = a;
	// this->b = b;
	// this->c = c;
	// this->d = d;

	drawing_data << a << b << c << d;
	drawing_commands << SetTransformation;
}

void VectorDisplay::setScale (qreal f) noexcept
{
	// set HW transformation: scaled only: undistorted and not rotated
	// value should be in range -1 … +1

	// a = d = f;
	// b = c = 0;

	drawing_data << f;
	drawing_commands << SetScale;
}

void VectorDisplay::setScaleAndRotation (qreal f, qreal rad) noexcept
{
	// set HW transformation: undistorted: scaled and rotated
	// scaling should be in range -1 … +1
	// rotation is ccw and should be in range -2PI … +2PI

	// a = d = f * cos(rad);
	// b =     f * sin(rad);
	// c = -b;

	drawing_data << f*sin(rad) << f*cos(rad);
	drawing_commands << SetScaleAndRotation;
}

void VectorDisplay::definePen (uint idx, QPen pen) noexcept
{
	assert(idx<256);

	while (pens.count() <= idx) { pens << display.default_pen; }
	pens[idx] = pen;
}

void VectorDisplay::definePen (uint idx, QColor color, qreal width) noexcept
{
	definePen(idx, QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
}

void VectorDisplay::defineBrush (uint idx, QBrush brush) noexcept
{
	assert(idx<256);

	while (brushes.count() < idx) { brushes << display.default_brush; }
	brushes[idx] = brush;
}

void VectorDisplay::vsync() noexcept
{
	// move current frame to new_frame so that it can be displayed by paintEvent()
	// reclaim buffers from old_frame or new_frame, whichever seems allocated
	// clear current buffer

	pipeline_lock.lock();

	assert(old_drawing_commands.count()==0 || new_drawing_commands.count()==0);

	// swap old and new => old has the purged and new has the allocated buffers:
	if (old_drawing_commands.count())
	{
		std::swap(new_drawing_commands, old_drawing_commands);
		std::swap(new_drawing_data,     old_drawing_data);
	}

	// swap new and current => move current_frame to new_frame and reclaim new_frame buffers:
	std::swap(new_drawing_commands, drawing_commands);
	std::swap(new_drawing_data,     drawing_data);

	// current := 0
	drawing_commands.shrink(0);	// don't purge
	drawing_data.shrink(0);

	pipeline_lock.unlock();
	update();
}

void VectorDisplay::selectPen (uint idx) noexcept
{
	assert(idx<pens.count());

	drawing_commands << SelectPen << uint8(idx);
}

void VectorDisplay::moveTo (qreal x, qreal y) noexcept
{
	drawing_data << x << y;
	drawing_commands << MoveTo;
}

void VectorDisplay::drawTo (qreal x, qreal y) noexcept
{
	drawing_data << x << y;
	drawing_commands << DrawTo;
}

void VectorDisplay::moveTo (QPointF p) noexcept
{
	drawing_data << p.x() << p.y();
	drawing_commands << MoveTo;
}

void VectorDisplay::drawTo (QPointF p) noexcept
{
	drawing_data << p.x() << p.y();
	drawing_commands << DrawTo;
}

void VectorDisplay::move (qreal dx, qreal dy) noexcept
{
	drawing_data << dx << dy;
	drawing_commands << Move;
}

void VectorDisplay::draw (qreal dx, qreal dy) noexcept
{
	drawing_data << dx << dy;
	drawing_commands << Draw;
}

void VectorDisplay::drawHor (qreal dx) noexcept
{
	drawing_data << dx;
	drawing_commands << DrawHor;
}

void VectorDisplay::drawVer (qreal dy) noexcept
{
	drawing_data << dy;
	drawing_commands << DrawVer;
}

void VectorDisplay::setTextSize (qreal size, qreal width) noexcept
{
	assert(size >= 1 && size <= 256);
	assert(width >= 0.1 && width <= 8.0);	// 1.0 = undistorted

	uint h = uint(0.5 + size);				// height in px
	uint w = uint(0.5 + width * 32);		// rel. width: 32 = undistorted

	assert(h >= 1 && h <= 256);
	assert(w >= 1 && w <= 256);

	drawing_commands << SetTextSize << uint8(h-1) << uint8(w-1);
}

void VectorDisplay::print (cstr text, bool centered) noexcept
{
	assert(text);
	uint slen = uint(strlen(text));
	assert(slen<=255);

	drawing_commands << DrawText << uint8(centered);
	drawing_commands.append(reinterpret_cast<const uchar*>(text),slen+1);
}

void VectorDisplay::print (char c) noexcept
{
	assert(c != 0);

	drawing_commands << DrawText << uint8(c) << 0;
}

void VectorDisplay::drawLine (qreal x1, qreal y1, qreal x2, qreal y2) noexcept
{
	drawing_data << x1 << y1 << x2 << y2;
	drawing_commands << DrawLine;
}

void VectorDisplay::drawLine (QPointF p1, QPointF p2) noexcept
{
	drawing_data << p1.x() << p1.y() << p2.x() << p2.y();
	drawing_commands << DrawLine;
}

void VectorDisplay::drawLine (QLineF line) noexcept
{
	drawing_data << line.p1().x() << line.p1().y() << line.p2().x() << line.p2().y();
	drawing_commands << DrawLine;
}

void VectorDisplay::drawPolyline (const QPointF* points, uint count) noexcept
{
	assert (count >= 2 && count <= 255);

	drawing_data.append(reinterpret_cast<const qreal*>(points), count * 2);
	drawing_commands << DrawPolyline << uint8(count);
}

void VectorDisplay::drawLines(const QLineF* lines, uint count) noexcept
{
	// draw bunch of lines

	assert(count >= 1 && count <= 255);

	static_assert (sizeof(QLineF) == 4 * sizeof(qreal), "");
	drawing_data.append(reinterpret_cast<const qreal*>(lines), count*4);
	drawing_commands << DrawLines << uint8(count);
}

void VectorDisplay::drawRectangle (qreal x1, qreal y1, qreal x2, qreal y2) noexcept
{
	// make valid:
	if (x1 > x2) std::swap(x1,x2);
	if (y1 > y2) std::swap(y1,y2);

	drawing_data << x1 << y1 << x2-x1 << y2-y1;		// x,y, w,h
	drawing_commands << DrawRectangle;
}

void VectorDisplay::drawRectangle (const QPointF& a, const QPointF& b) noexcept
{
	drawRectangle(a.x(), a.y(), b.x(), b.y());
}

void VectorDisplay::drawRectangle (const QRectF& box) noexcept
{
	if (box.isValid())
	{
		static_assert (sizeof(box) == 4*sizeof(qreal), "");
		drawing_data.append(reinterpret_cast<const qreal*>(&box),4);
		drawing_commands << DrawRectangle;
	}
}

void VectorDisplay::drawRectangle (const QPointF& p, const QSizeF& sz) noexcept
{
	if (sz.isValid())
	{
		drawing_data << p.x() << p.y() << sz.width() << sz.height();
		drawing_commands << DrawRectangle;
	}
}

void VectorDisplay::drawTriangle (const QPointF& a, const QPointF& b, const QPointF& c) noexcept
{
	drawing_data << a.x() << a.y() << b.x() << b.y() << c.x() << c.y();
	drawing_commands << DrawTriangle;
}

void VectorDisplay::drawPolygon (const QPointF* points, uint count) noexcept
{
	assert(count <= 255);

	static_assert (sizeof(QPointF) == 2 * sizeof(qreal), "");
	drawing_data.append(reinterpret_cast<const qreal*>(points), count*2);
	drawing_commands << DrawPolygon << uint8(count);
}

void VectorDisplay::drawPolygon (const Array<QPointF>& points) noexcept
{
	drawPolygon(points.getData(), points.count());
}

void VectorDisplay::drawConvexPolygon (const QPointF* points, uint count) noexcept
{
	assert(count <= 255);

	static_assert (sizeof(QPointF) == 2 * sizeof(qreal), "");
	drawing_data.append(reinterpret_cast<const qreal*>(points), count*2);
	drawing_commands << DrawConvexPolygon << uint8(count);
}

void VectorDisplay::drawConvexPolygon (const Array<QPointF>& points) noexcept
{
	drawConvexPolygon(points.getData(), points.count());
}

void VectorDisplay::drawEllipse (const QPointF& center, qreal rx, qreal ry) noexcept
{
	drawing_data << center.x() << center.y() << rx << ry;
	drawing_commands << DrawEllipse;
}


// =================================================
//					Engine 1
// =================================================

Engine1::Engine1 (VectorDisplay* d, uint vram_size, uint8* ram) :
	display(d), now(0), vram_mask(0), vram(ram),
	vram_self_allocated(!ram),
	A(0), X(0),Y(0), R(0),I(0),C(0),E(0)
{
	// normally ram should be allocated by the host.
	// then call ctor with size and address.
	// else call ctor with size and NULL.
	// size must be 2^N, 8kB/16kB (for 4096/8192 points) is recommended.

	assert((vram_size & (vram_size-1)) == 0);	// must be 2^N
	if (!ram) vram = new uint8[vram_size];
	vram_mask = vram_size - 1;

	display->setDisplaySize(QSize(255,255));	// set logical size
}

void Engine1::reset (uint32 cc)
{
	// Push Reset
	// disable drawing until setAddress() is called

	if((0)) run(cc);	// for best emulation
	else  now = cc;		// for everyone else

	E = false;			// stop clock		(required)
	C = false;			// beam off			(safety)
	I = false;			// no interrupt		(required)

	// X = DAC counters are not reset!
	// Y = DAC counters are not reset!
}

void Engine1::start (uint32 cc, uint address)
{
	// Start a new frame:
	// reset address for drawing engine and start drawing a frame.
	// for reading draw commands from ram the 'machine' must call run(cc).
	// for making the current frame 'valid', the machine must call ffb(cc).

	if((0)) run(cc);	// for best emulation
	else  now = cc;		// for everyone else

	A = address;
	R = true;			// next (first) command: relative address or control code
	I = false;			// interrupt off
	C = true;			// beam on
	E = true;			// enable drawing

	// X = DAC counters are not reset!
	// Y = DAC counters are not reset!
}

void Engine1::vsync (uint32 cc)
{
	// 'Frame Flyback'
	// Finish a frame and rewind time base by cc
	// move wbu[] to rbu[] for paintEvent()
	// call update()		for paintEvent()

	run(cc);		// catch up
	now -= cc;		// rewind time base

	// if synchronization with vblank is enabled, then this is blocking:
	display->vsync();
}

bool Engine1::run (uint32 cc)
{
	// Run Co-routine up to clock cycle cc.
	// returns value of register E:
	// 	 true  = still drawing
	// 	 false = end of frame

	if (!E) { now=cc; return false; }

	while (now < cc)
	{
		// read 2 bytes from ram:
		now += 6;
		A &= vram_mask;
		int x = int8(vram[A++]);
		int y = int8(vram[A++]);

		if (R)			// relative: draw, move or control code:
		{
			if (x == int8(CMD))		// control code
			{
				R = y & 8;
			//	I = y & 4;			// interrupt = I=1 && E=0
				C = y & 2;
				E = y & 1;

				if (E) continue;

				I = y & 4;			// interrupt = I=1 && E=0
				return false;		// end of frame
			}

			// draw or move relative:

			now += max(uint(x) & 0x7F, uint(y) & 0x7F);	// consume time

			if (x & 0x80) x = -(x & 0x7F);	// x and y are stored as
			if (y & 0x80) y = -(y & 0x7F);	// bit7 = sign, bits0…6 = absolute value

			X = int8(X+x);			// TODO: actual HW will draw lines differently
			Y = int8(Y+y);			//		 on overflow

			if (C)	// draw visible line
			{
				display->drawTo(X,Y);
				continue;
			}
			//else draw invisible line to x,y
		}
		else // set DAC counters directly to x,y
		{
			X = x;
			Y = y;
		}

		display->moveTo(X,Y);

		R = true;
		C = true;
	}

	return true;	// still drawing
}

static void draw_next_engine1_demo_frame (Engine1* engine)
{
	VectorDisplay* display = engine->display;

	// Draw test corners:

	display->drawLine(-128,-128,-128,-118);
	display->drawLine(-128,-128,-118,-128);
	display->drawLine(+127,+127,+127,+117);
	display->drawLine(+127,+127,+117,+127);

	// Rotate display:

	static qreal rad = 0;
	rad -= M_PI/180*0.1;
	if (rad<0) rad += 2*M_PI;
	display->setScaleAndRotation(0.9,rad);

	// Print some text:

	display->moveTo(-90,-50);
	display->setTextSize(20,12.0/20);
	display->print("Hallo Welt, hier kommt das Vektor-Display!");
	display->moveTo(-90,-70);
	display->setTextSize(16,12.0/16);
	display->print(fromutf8str("[\\]{|}ÄÖÜäöüß@$%&?oO0iIlL12345S678B9"));
	display->moveTo(-90,-88);
	display->setTextSize(12,0.9);
	display->print(fromutf8str("AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz"));

	// Draw boxes and lines using the engine:

	static const uint8 program[] =
	{
		#define DRAW(dx,dy)	dx<0 ? 0x80 - dx : uint8(dx), dy<0 ? 0x80 - dy : uint8(dy)
		#define	MOVE(dx,dy) Engine1::CMD, Engine1::NOP + Engine1::Eon + Engine1::Rrel, DRAW(dx,dy)
		#define MOVETO(x,y)	Engine1::CMD, Engine1::NOP + Engine1::Eon + Engine1::Rabs, uint8(x), uint8(y)
		#define END_FRAME	Engine1::CMD, Engine1::NOP
		#define END_WITH_I	Engine1::CMD, Engine1::NOP + Engine1::Ion
		#define BOX(W)				\
			MOVETO(-W,-W),			\
			DRAW(W,0), DRAW(W,0),	\
			MOVETO(-W,-W),			\
			DRAW(W,0), DRAW(W,0),	\
			MOVETO(-W,-W),			\
			DRAW(W,0), DRAW(W,0),	\
			DRAW(0,W), DRAW(0,W),	\
			DRAW(-W,0),DRAW(-W,0),	\
			DRAW(0,-W),DRAW(0,-W)	\

		BOX(100),
		BOX(99),

		MOVE(+6,+6),		// -97,-97
		DRAW(93,0),DRAW(93,0),

		MOVETO(0,0),
		DRAW(120,0),

		BOX(97),
		BOX(15),

		END_WITH_I
	};

	memcpy(engine->videoRam(), program, sizeof(program));
	engine->start(0,0);
	engine->vsync(10000);
}

void runEngine1Demo (VectorDisplay* d)
{
	Engine1* engine = new Engine1(d);

	QTimer* timer = new QTimer();
	QObject::connect(timer, &QTimer::timeout, [=]
	{
		draw_next_engine1_demo_frame(engine);
	});

	bool f = d->synchronizesWithVblank();
	timer->setInterval(f?0:17);				// 17 ~ 1/60 = 16.666
	timer->start();

	QObject::connect(d, &QWidget::destroyed, [=]
	{
		timer->stop();
		timer->deleteLater();
		delete engine;
	});
}

static inline qreal torad (qreal degree) { return M_PI/180 * degree; }

static void draw_next_clock_demo_frame (VectorDisplay* display)
{
	// draw clock

	display->selectPen(6);
	display->drawCircle({0,0},90+3);
	display->drawCircle({0,0},90-3);

	#define P(W,R) {sin(torad(W))*(R), cos(torad(W))*(R)}
	#define L(W) {P(W,85),P(W,95)}
	static const QLineF outer[] =
	{
		L(0-1), L(0), L(0+1),  L(30),L(60),
		L(90-1),L(90),L(90+1), L(120),L(150),
		L(180-1),L(180),L(180+1), L(210),L(240),
		L(270-1),L(270),L(270+1), L(300),L(330)
	};
	display->selectPen(0);
	display->drawLines(outer,NELEM(outer));
	#undef P
	#undef L

	display->setTextSize(12);
	for (uint pen=3; pen<5; pen++)
	{
		display->selectPen(pen);
		for (uint hr=1; hr<=12; hr++)
		{
			qreal rad = 2*M_PI * hr / 12;
			display->moveTo(sin(rad)*75-3, cos(rad)*75-4);
			display->print(numstr(hr));
		}
	}

	// get current time:

	timespec tv = now<timespec>();
	struct tm tm;
	localtime_r(&tv.tv_sec, &tm);

	// draw date

	static const cstr month[12] = {"Jan","Feb","Mär","Apr","Mai","Jun","Jul","Aug","Sep","Okt","Nov","Dez"};
	static const cstr wotag[7] = {"So","Mo","Di","Mi","Do","Fr","Sa"};
	cstr datestr = usingstr("%s, %i. %s. %i", wotag[tm.tm_wday],tm.tm_mday,fromutf8str(month[tm.tm_mon]),1900+tm.tm_year);
	display->setTextSize(12,0.9);
	display->selectPen(0);
	display->moveTo(0,-30);
	display->print(datestr,1/*centered*/);

	// calculate angles for clock hands:

	double s_deg = tm.tm_sec  * 6;
	double m_deg = tm.tm_min  * 6  + tm.tm_sec * 0.1;
	double h_deg = tm.tm_hour * 30 + tm.tm_min * 0.5;

	// adjust seconds hand angle for animation:

	switch(tv.tv_nsec*60/1000000000)
	{
	case 0: s_deg -= 5; break;
	case 1: s_deg -= 4; break;
	case 2: s_deg -= 3; break;
	case 3: s_deg -= 2; break;
	case 4: s_deg -= 1; break;
	case 5: s_deg -= 0; break;
	case 6: s_deg += 1; break;
	case 7: s_deg += 1; break;
	}

	// rotate clock coords and draw hour hand:
	// length = 55

	display->setScaleAndRotation(1.0, torad(h_deg-90));
	display->selectPen(2);				// phosphorous
	static const QPointF sh_lumi[] = {{10,+1},{40,0},{10,-1}};
	display->drawPolygon(sh_lumi,3);
	display->selectPen(0);
	static const QPointF hh[]{{-10,-4},{-10,+4},{55,0}};
	display->drawConvexPolygon(hh,3);

	// rotate clock coords and draw minute hand:
	// length = 80

	display->setScaleAndRotation(1.0, torad(m_deg-90));
	display->selectPen(2);				// phosphorous
	display->drawLine(10,0,60,0);
	display->selectPen(0);
	static const QPointF mh[]{{-10,-2},{-10,+2},{80,0}};
	display->drawConvexPolygon(mh,3);

	// rotate clock coords and draw seconds hand:
	// length = 90

	display->setScaleAndRotation(1.0, torad(s_deg-90));
	display->selectPen(1);				// redish

	display->drawCircle({0,0},5);
	display->drawLine(-10,0,-5,0);
	display->drawLine(+5,0,+90,0);

	display->vsync();
}

void runClockDemo (VectorDisplay* display)
{
	display->setDisplaySize(QSize(200,200));
	display->definePen(1,QColor(0xff,0x33,0x33,0x80),1.5);	// red seconds handle
	display->definePen(2,QColor(0x33,0xff,0x33,0xc0),2.0);	// luminance on hr & minutes handle
	display->definePen(3,QColor(0xff,0xff,0xff,0x80),3.0);	// outline numbers
	display->definePen(4,QColor(0x00,0x00,0x00,0xc0),1.5);	// outline fill on numbers
	display->definePen(5,QColor(0x33,0xff,0x33,0x80),1.5);	// luminance on numbers
	display->definePen(6,QColor(0x00,0x44,0xff,0x60),2);	// blue ring


	QTimer* timer = new QTimer();
	QObject::connect(timer, &QTimer::timeout, [=]
	{
		draw_next_clock_demo_frame(display);
	});

	bool f = display->synchronizesWithVblank();
	timer->setInterval(f?0:17);				// 17 ~ 1/60 = 16.666
	timer->start();

	QObject::connect(display, &QWidget::destroyed, [=]
	{
		timer->stop();
		timer->deleteLater();
	});
}








































