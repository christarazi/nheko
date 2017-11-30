/*
 * nheko Copyright (C) 2017  Konstantinos Sideris <siderisk@auth.gr>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QBrush>
#include <QDebug>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QPainter>
#include <QPixmap>

#include "timeline/widgets/FileItem.h"

namespace events = matrix::events;
namespace msgs   = matrix::events::messages;

void
FileItem::init()
{
        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);

        icon_.addFile(":/icons/icons/ui/arrow-pointing-down.png");

        QList<QString> url_parts = url_.toString().split("mxc://");
        if (url_parts.size() != 2) {
                qDebug() << "Invalid format for image" << url_.toString();
                return;
        }

        QString media_params = url_parts[1];
        url_                 = QString("%1/_matrix/media/r0/download/%2")
                 .arg(client_.data()->getHomeServer().toString(), media_params);

        connect(client_.data(), &MatrixClient::fileDownloaded, this, &FileItem::fileDownloaded);
}

FileItem::FileItem(QSharedPointer<MatrixClient> client,
                   const events::MessageEvent<msgs::File> &event,
                   QWidget *parent)
  : QWidget(parent)
  , url_{event.msgContent().url()}
  , text_{event.content().body()}
  , event_{event}
  , client_{client}
{
        readableFileSize_ = calculateFileSize(event.msgContent().info().size);

        init();
}

FileItem::FileItem(QSharedPointer<MatrixClient> client,
                   const QString &url,
                   const QString &filename,
                   QWidget *parent)
  : QWidget(parent)
  , url_{url}
  , text_{QFileInfo(filename).fileName()}
  , client_{client}
{
        readableFileSize_ = calculateFileSize(QFileInfo(filename).size());

        init();
}

QString
FileItem::calculateFileSize(int nbytes) const
{
        if (nbytes < 1024)
                return QString("%1 B").arg(nbytes);

        if (nbytes < 1024 * 1024)
                return QString("%1 KB").arg(nbytes / 1024);

        return QString("%1 MB").arg(nbytes / 1024 / 1024);
}

void
FileItem::openUrl()
{
        if (url_.toString().isEmpty())
                return;

        if (!QDesktopServices::openUrl(url_))
                qWarning() << "Could not open url" << url_.toString();
}

QSize
FileItem::sizeHint() const
{
        return QSize(MaxWidth, Height);
}

void
FileItem::mousePressEvent(QMouseEvent *event)
{
        if (event->button() != Qt::LeftButton)
                return;

        auto point = event->pos();

        // Click on the download icon.
        if (QRect(HorizontalPadding, VerticalPadding / 2, IconDiameter, IconDiameter)
              .contains(point)) {
                filenameToSave_ = QFileDialog::getSaveFileName(this, tr("Save File"), text_);

                if (filenameToSave_.isEmpty())
                        return;

                client_->downloadFile(event_.eventId(), url_);
        } else {
                openUrl();
        }
}

void
FileItem::fileDownloaded(const QString &event_id, const QByteArray &data)
{
        if (event_id != event_.eventId())
                return;

        try {
                QFile file(filenameToSave_);

                if (!file.open(QIODevice::WriteOnly))
                        return;

                file.write(data);
                file.close();
        } catch (const std::exception &ex) {
                qDebug() << "Error while saving file to:" << ex.what();
        }
}

void
FileItem::paintEvent(QPaintEvent *event)
{
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QFont font("Open Sans");
        font.setPixelSize(12);
        font.setWeight(80);

        QFontMetrics fm(font);

        int computedWidth = std::min(
          fm.width(text_) + 2 * IconRadius + VerticalPadding * 2 + TextPadding, (double)MaxWidth);

        QPainterPath path;
        path.addRoundedRect(QRectF(0, 0, computedWidth, Height), 10, 10);

        painter.setPen(Qt::NoPen);
        painter.fillPath(path, backgroundColor_);
        painter.drawPath(path);

        QPainterPath circle;
        circle.addEllipse(QPoint(IconXCenter, IconYCenter), IconRadius, IconRadius);

        painter.setPen(Qt::NoPen);
        painter.fillPath(circle, iconColor_);
        painter.drawPath(circle);

        icon_.paint(&painter,
                    QRect(IconXCenter - DownloadIconRadius / 2,
                          IconYCenter - DownloadIconRadius / 2,
                          DownloadIconRadius,
                          DownloadIconRadius),
                    Qt::AlignCenter,
                    QIcon::Normal);

        const int textStartX = HorizontalPadding + 2 * IconRadius + TextPadding;
        const int textStartY = VerticalPadding + fm.ascent() / 2;

        // Draw the filename.
        QString elidedText =
          fm.elidedText(text_,
                        Qt::ElideRight,
                        computedWidth - HorizontalPadding * 2 - TextPadding - 2 * IconRadius);

        painter.setFont(font);
        painter.setPen(QPen(textColor_));
        painter.drawText(QPoint(textStartX, textStartY), elidedText);

        // Draw the filesize.
        font.setWeight(50);
        painter.setFont(font);
        painter.setPen(QPen(textColor_));
        painter.drawText(QPoint(textStartX, textStartY + 1.5 * fm.ascent()), readableFileSize_);
}