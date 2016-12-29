/****************************************************************************
 * This file is part of Liri.
 *
 * Copyright (C) 2015-2016 Pier Luigi Fiorini
 *
 * Author(s):
 *    Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
 *
 * $BEGIN_LICENSE:LGPL2.1+$
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * $END_LICENSE$
 ***************************************************************************/

/*
 * This code takes inspiration from the xwayland module from Weston,
 * which is:
 *
 * Copyright (C) 2011 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <QtWaylandCompositor/QWaylandCompositor>

#include "xcbwrapper.h"
#include "xcbresources.h"
#include "xwayland.h"
#include "xwaylandmanager.h"
#include "xwaylandshellsurface.h"
#include "xwaylandquickshellintegration.h"

#define TYPE_WM_PROTOCOLS       XCB_ATOM_CUT_BUFFER0
#define TYPE_MOTIF_WM_HINTS     XCB_ATOM_CUT_BUFFER1
#define TYPE_NET_WM_STATE       XCB_ATOM_CUT_BUFFER2
#define TYPE_WM_NORMAL_HINTS    XCB_ATOM_CUT_BUFFER3

XWaylandShellSurface::XWaylandShellSurface(xcb_window_t window, const QRect &geometry,
                                           bool overrideRedirect, XWaylandManager *parent)
    : QObject(parent)
    , m_wm(parent)
    , m_window(window)
    , m_geometry(geometry)
    , m_propsDirty(true)
    , m_overrideRedirect(overrideRedirect)
    , m_transientFor(nullptr)
    , m_windowType(Qt::WindowType::Window)
    , m_surfaceId(0)
    , m_surface(nullptr)
    , m_wmState(WithdrawnState)
    , m_workspace(0)
    , m_activated(false)
    , m_maximized(false)
    , m_fullscreen(false)
{
    m_properties.deleteWindow = 0;

    xcb_get_geometry_cookie_t cookie =
            xcb_get_geometry(Xcb::connection(), window);

    quint32 values[1];
    values[0] = XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_change_window_attributes(Xcb::connection(), window,
                                 XCB_CW_EVENT_MASK, values);

    xcb_get_geometry_reply_t *reply =
            xcb_get_geometry_reply(Xcb::connection(), cookie, Q_NULLPTR);
    if (reply)
        m_hasAlpha = reply->depth == 32;
    else
        m_hasAlpha = false;
    free(reply);

    m_wm->addWindow(m_window, this);
}

XWaylandShellSurface::~XWaylandShellSurface()
{
    m_wm->removeWindow(m_window);
}

Qt::WindowType XWaylandShellSurface::windowType() const
{
    return m_windowType;
}

quint32 XWaylandShellSurface::surfaceId() const
{
    return m_surfaceId;
}

void XWaylandShellSurface::setSurfaceId(quint32 id)
{
    if (m_surfaceId > 0)
        return;

    m_surfaceId = id;
}

QWaylandSurface *XWaylandShellSurface::surface() const
{
    return m_surface;
}

void XWaylandShellSurface::setSurface(QWaylandSurface *surface)
{
    if (m_surface)
        disconnect(m_surface, &QWaylandSurface::surfaceDestroyed,
                   this, &XWaylandShellSurface::handleSurfaceDestroyed);

    m_surface = surface;
    qCDebug(XWAYLAND_TRACE) << "Assign surface" << surface << "to shell surface for" << m_window;

    if (m_surface) {
        connect(m_surface, &QWaylandSurface::surfaceDestroyed,
                this, &XWaylandShellSurface::handleSurfaceDestroyed);
        readProperties();

        Q_EMIT m_wm->shellSurfaceAdded(this);
        Q_EMIT surfaceChanged();
        //Q_EMIT setGeometry(m_geometry);
    } else {
        m_wm->removeWindow(m_window);
    }
}

QString XWaylandShellSurface::appId() const
{
    return m_properties.appId;
}

QString XWaylandShellSurface::title() const
{
    return m_properties.title;
}

bool XWaylandShellSurface::activated() const
{
    return m_activated;
}

bool XWaylandShellSurface::maximized() const
{
    return m_maximized;
}

bool XWaylandShellSurface::fullscreen() const
{
    return m_fullscreen;
}

void XWaylandShellSurface::setWmState(WmState state)
{
    quint32 property[2];
    property[0] = state;
    property[1] = XCB_WINDOW_NONE;

    xcb_change_property(Xcb::connection(),
                        XCB_PROP_MODE_REPLACE,
                        m_window,
                        Xcb::resources()->atoms->wm_state,
                        Xcb::resources()->atoms->wm_state,
                        32, 2, property);
}

void XWaylandShellSurface::setNetWmState()
{
    quint32 property[3];
    quint32 i = 0;

    if (m_fullscreen)
        property[i++] = Xcb::resources()->atoms->net_wm_state_fullscreen;
    if (m_maximized) {
        property[i++] = Xcb::resources()->atoms->net_wm_state_maximized_horz;
        property[i++] = Xcb::resources()->atoms->net_wm_state_maximized_vert;
    }

    xcb_change_property(Xcb::connection(),
                        XCB_PROP_MODE_REPLACE,
                        m_window,
                        Xcb::resources()->atoms->net_wm_state,
                        XCB_ATOM_ATOM,
                        32, i, property);
}

void XWaylandShellSurface::setWorkspace(int workspace)
{
    // Passing a workspace < 0 deletes the property
    if (workspace >= 0) {
        xcb_change_property(Xcb::connection(),
                            XCB_PROP_MODE_REPLACE,
                            m_window,
                            Xcb::resources()->atoms->net_wm_desktop,
                            XCB_ATOM_CARDINAL,
                            31, 1, &workspace);
    } else {
        xcb_delete_property(Xcb::connection(),
                            m_window, Xcb::resources()->atoms->net_wm_desktop);
    }
}

void XWaylandShellSurface::readProperties()
{
    if (!m_propsDirty)
        return;
    m_propsDirty = false;

    QMap<xcb_atom_t, xcb_atom_t> props;
    props[XCB_ATOM_WM_CLASS] = XCB_ATOM_STRING;
    props[XCB_ATOM_WM_NAME] = XCB_ATOM_STRING;
    props[XCB_ATOM_WM_TRANSIENT_FOR] = XCB_ATOM_WINDOW;
    props[Xcb::resources()->atoms->wm_protocols] = TYPE_WM_PROTOCOLS;
    props[Xcb::resources()->atoms->wm_normal_hints] = TYPE_WM_NORMAL_HINTS;
    props[Xcb::resources()->atoms->net_wm_state] = TYPE_NET_WM_STATE;
    props[Xcb::resources()->atoms->net_wm_window_type] = XCB_ATOM_ATOM;
    props[Xcb::resources()->atoms->net_wm_name] = XCB_ATOM_STRING;
    props[Xcb::resources()->atoms->net_wm_pid] = XCB_ATOM_CARDINAL;
    props[Xcb::resources()->atoms->motif_wm_hints] = TYPE_MOTIF_WM_HINTS;
    props[Xcb::resources()->atoms->wm_client_machine] = XCB_ATOM_WM_CLIENT_MACHINE;

    QMap<xcb_atom_t, xcb_get_property_cookie_t> cookies;
    Q_FOREACH (xcb_atom_t atom, props.keys()) {
        xcb_get_property_cookie_t cookie = xcb_get_property(
                    Xcb::connection(), 0, m_window, atom, XCB_ATOM_ANY, 0, 2048);
        cookies[atom] = cookie;
    }

    m_sizeHints.flags = 0;
    m_motifHints.flags = 0;
    m_properties.deleteWindow = 0;

    Q_FOREACH (xcb_atom_t atom, props.keys()) {
        xcb_get_property_reply_t *reply =
                xcb_get_property_reply(Xcb::connection(), cookies[atom], Q_NULLPTR);
        if (!reply)
            // Bad window, usually
            continue;
        if (reply->type == XCB_ATOM_NONE) {
            // No such property
            free(reply);
            continue;
        }

        void *p = decodeProperty(props[atom], reply);
        dumpProperty(atom, reply);

        switch (atom) {
        case XCB_ATOM_WM_CLASS:
            m_properties.appId = QString::fromUtf8((char *)p);
            free(p);
            Q_EMIT appIdChanged();
            break;
        case XCB_ATOM_WM_NAME:
            m_properties.title = QString::fromUtf8((char *)p);
            free(p);
            Q_EMIT titleChanged();
            break;
        case XCB_ATOM_WM_TRANSIENT_FOR:
            m_transientFor = (XWaylandShellSurface *)p;
            break;
        default:
            break;
        }

        free(reply);
    }
}

void XWaylandShellSurface::readAndDumpProperty(xcb_atom_t atom)
{
    xcb_get_property_cookie_t cookie =
            xcb_get_property(Xcb::connection(), 0, m_window,
                             atom, XCB_ATOM_ANY, 0, 2048);
    xcb_get_property_reply_t *reply =
            xcb_get_property_reply(Xcb::connection(), cookie, Q_NULLPTR);

    dumpProperty(atom, reply);

    free(reply);
}

void XWaylandShellSurface::resizeFrame(const QSize &size, quint32 mask, quint32 *values)
{
    xcb_configure_window(Xcb::connection(), m_window, mask, values);
}

QSize XWaylandShellSurface::sizeForResize(const QSizeF &initialSize, const QPointF &delta, ResizeEdge edges)
{
    return initialSize.toSize();
}

void XWaylandShellSurface::sendConfigure(const QSize &size)
{
    m_geometry.setSize(size);

    xcb_configure_notify_event_t configure_notify;
    configure_notify.response_type = XCB_CONFIGURE_NOTIFY;
    configure_notify.pad0 = 0;
    configure_notify.event = m_window;
    configure_notify.window = m_window;
    configure_notify.above_sibling = XCB_WINDOW_NONE;
    configure_notify.x = m_geometry.topLeft().x();
    configure_notify.y = m_geometry.topLeft().y();
    configure_notify.width = m_geometry.size().width();
    configure_notify.height = m_geometry.size().height();
    configure_notify.border_width = 0;
    configure_notify.override_redirect = 0;
    configure_notify.pad1 = 0;

    xcb_send_event(Xcb::connection(), 0, m_window,
                   XCB_EVENT_MASK_STRUCTURE_NOTIFY,
                   (char *)&configure_notify);
}

void XWaylandShellSurface::map()
{
    xcb_map_window(Xcb::connection(), m_window);
    Q_EMIT mapped();
}

void XWaylandShellSurface::unmap()
{
    Q_EMIT unmapped();
}

void XWaylandShellSurface::moveTo(const QPoint &pos)
{
    m_geometry.setTopLeft(pos);
    Q_EMIT setGeometry(m_geometry);
}

void XWaylandShellSurface::resize(const QSize &size)
{
    m_geometry.setSize(size);
    Q_EMIT setGeometry(m_geometry);
}

XWaylandQuickShellIntegration *XWaylandShellSurface::createIntegration(XWaylandQuickShellSurfaceItem *item)
{
    return new XWaylandQuickShellIntegration(item);
}

XWaylandShellSurface::operator xcb_window_t() const
{
    return m_window;
}

xcb_window_t XWaylandShellSurface::window() const
{
    return m_window;
}

void XWaylandShellSurface::dumpProperty(xcb_atom_t property, xcb_get_property_reply_t *reply)
{
    QString buffer = QStringLiteral("\t%1: ").arg(Xcb::Atom::nameFromAtom(property));

    if (!reply) {
        buffer += QLatin1String("(no reply)");
        qCDebug(XWAYLAND_TRACE) << qPrintable(buffer);
        return;
    }

    buffer += QString().sprintf("%s/%d, length %d (value_len %d): ",
                                qPrintable(Xcb::Atom::nameFromAtom(reply->type)),
                                reply->format,
                                xcb_get_property_value_length(reply),
                                reply->value_len);

    if (reply->type == Xcb::resources()->atoms->incr) {
        qint32 *v = (qint32 *)xcb_get_property_value(reply);
        buffer += QString().sprintf("%d", *v);
    } else if (reply->type == Xcb::resources()->atoms->utf8_string ||
               reply->type == Xcb::resources()->atoms->string) {
        const char *v = (const char *)xcb_get_property_value(reply);
        int len = reply->value_len > 40 ? 40 : reply->value_len;
        buffer += QString().sprintf("\"%.*s\"", len, v);
    } else if (reply->type == XCB_ATOM_ATOM) {
        xcb_atom_t *v = (xcb_atom_t *)xcb_get_property_value(reply);

        for (quint32 i = 0; i < reply->value_len; i++) {
            QString name = Xcb::Atom::nameFromAtom(v[i]);
            if (i > 0)
                buffer += buffer.sprintf(", %s", qPrintable(name));
            else
                buffer += name;
        }
    } else {
        buffer += QStringLiteral("huh?");
    }

    qCDebug(XWAYLAND_TRACE) << qPrintable(buffer);
}

void *XWaylandShellSurface::decodeProperty(xcb_atom_t type, xcb_get_property_reply_t *reply)
{
    switch (type) {
    case XCB_ATOM_WM_CLIENT_MACHINE:
    case XCB_ATOM_STRING: {
        char *p = strndup((char *)xcb_get_property_value(reply),
                          xcb_get_property_value_length(reply));
        return p;
    }
    case XCB_ATOM_WINDOW: {
        xcb_window_t *xid = (xcb_window_t *)xcb_get_property_value(reply);
        return m_wm->shellSurfaceFromId(*xid);
    }
    case XCB_ATOM_CARDINAL:
    case XCB_ATOM_ATOM:
        return xcb_get_property_value(reply);
    case TYPE_WM_PROTOCOLS: {
        xcb_atom_t *value = (xcb_atom_t *)xcb_get_property_value(reply);
        for (uint32_t i = 0; i < reply->value_len; i++)
            if (value[i] == Xcb::resources()->atoms->wm_delete_window)
                m_properties.deleteWindow = 1;
        break;
    }
    case TYPE_WM_NORMAL_HINTS:
        memcpy(&m_sizeHints,
               xcb_get_property_value(reply),
               sizeof m_sizeHints);
        break;
    case TYPE_NET_WM_STATE: {
        xcb_atom_t *value = (xcb_atom_t *)xcb_get_property_value(reply);
        uint32_t i;
        for (i = 0; i < reply->value_len; i++) {
            if (value[i] == Xcb::resources()->atoms->net_wm_state_fullscreen && !m_fullscreen) {
                m_fullscreen = true;
                Q_EMIT fullscreenChanged();
            }
        }
        if (value[i] == Xcb::resources()->atoms->net_wm_state_maximized_horz && !m_maximized) {
            m_maximized = true;
            Q_EMIT maximizedChanged();
        }
        if (value[i] == Xcb::resources()->atoms->net_wm_state_maximized_vert && !m_maximized) {
            m_maximized = true;
            Q_EMIT maximizedChanged();
        }
        break;
    }
    case TYPE_MOTIF_WM_HINTS:
        memcpy(&m_motifHints,
               xcb_get_property_value(reply),
               sizeof m_motifHints);
        //if (m_motifHints.flags & MWM_HINTS_DECORATIONS)
        //m_decorated = m_motifHints.decorations;
        break;
    default:
        break;
    }

    return Q_NULLPTR;
}

void XWaylandShellSurface::handleSurfaceDestroyed()
{
    qCWarning(XWAYLAND) << "Surface paired with window" << m_window << "destroyed";

    m_surface = nullptr;
    Q_EMIT surfaceChanged();
    Q_EMIT surfaceDestroyed();
}

#include "moc_xwaylandshellsurface.cpp"
