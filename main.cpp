#include <QtGui>

int main(int argc, char **argv)
{
    QApplication a(argc, argv);
    QRect r;
    QImage image;
    QString out;
    for (int i=1; i<argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "-o") {
            if (i + 1 < argc) {
                out = argv[++i];
            } else {
                qDebug() << "bullshit";
                return 1;
            }
        } else if (image.isNull()) {
            if (!image.load(argv[i])) {
                qDebug() << "Couldn't load image" << argv[i];
                return 1;
            }
        } else if (r.isNull()) {
            QRegExp rx("([0-9]+),([0-9]+)\\+([0-9]+)x([0-9]+)");
            if (!rx.exactMatch(arg)) {
                qDebug() << "Can't parse rect" << arg;
                return 1;
            } else {
                r = QRect(rx.cap(1).toInt(), rx.cap(2).toInt(), rx.cap(3).toInt(), rx.cap(4).toInt());
            }
        } else {
            qDebug() << "Too many args";
        }
    }
    if (image.isNull() || r.isNull()) {
        qDebug() << "Too few args";
        return 1;
    }

    if (!out.isEmpty()) {
        return image.copy(r).save(out) ? 0 : 1;
    }

    QLabel label;
    label.setPixmap(QPixmap::fromImage(image.copy(r)));
    label.setAlignment(Qt::AlignCenter);
    new QShortcut(QKeySequence(Qt::Key_Q), &label, SLOT(close()));
    new QShortcut(QKeySequence(Qt::Key_Escape), &label, SLOT(close()));
    label.show();
    return a.exec();
}
