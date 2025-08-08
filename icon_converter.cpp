#include <QApplication>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QSvgRenderer>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Load SVG and convert to PNG
    QSvgRenderer renderer(QString(":/app_icon.svg"));
    
    // Create different sizes
    QList<int> sizes = {16, 22, 24, 32, 48, 64, 128, 256};
    
    for (int size : sizes) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        
        QPainter painter(&pixmap);
        renderer.render(&painter);
        painter.end();
        
        QString filename = QString("resources/app_icon_%1x%1.png").arg(size);
        pixmap.save(filename, "PNG");
        qDebug() << "Created" << filename;
    }
    
    qDebug() << "Icon conversion complete!";
    return 0;
}
