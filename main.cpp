#include <QtGui>

int main(int argc, char **argv)
{
    QApplication a(argc, argv);
    QRect r;
    QImage image;
    for (int i=1; i<argc; ++i) {
        if (image.isNull()) {
            if (!image.load(argv[i])) {
                return 1;
            }
        }
    }


    QImage dump;
    std::shared_ptr<Image> image1, image2;
    float threshold = 0;
    bool same = false;
    bool nojoin = false;
    bool dumpImages = false;
    int range = 2;
    for (int i=1; i<argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "--help" || arg == "-h") {
            usage(stdout);
            return 0;
        } else if (arg == "-v" || arg == "--verbose") {
            ++verbose;
        } else if (arg == "--imagemagick") {
            imageMagickFormat = true;
        } else if (arg == "--dump-images") {
            dumpImages = true;
        } else if (arg == "--no-join") {
            nojoin = true;
        } else if (arg.startsWith("--threshold=")) {
            bool ok;
            QString t = arg.mid(12);
            bool percent = false;
            if (t.endsWith("%")) {
                t.chop(1);
                percent = true;
            }
            threshold = t.toFloat(&ok);
            if (!ok || threshold < .0) {
                fprintf(stderr, "Invalid threshold (%s), must be positive float value\n",
                        qPrintable(arg.mid(12)));
                return 1;
            }
            if (percent) {
                threshold /= 100;
                threshold *= 256;
            }
            if (verbose)
                qDebug() << "threshold:" << threshold;
        } else if (arg.startsWith("--min-size=")) {
            bool ok;
            QString t = arg.mid(11);
            minSize = t.toInt(&ok);
            if (!ok || minSize <= 0) {
                fprintf(stderr, "Invalid --min-size (%s), must be positive integer value\n",
                        qPrintable(arg.mid(12)));
                return 1;
            }
            if (verbose)
                qDebug() << "min-size:" << minSize;
        } else if (arg == "--same") {
            same = true;
        } else if (arg.startsWith("--range=")) {
            bool ok;
            QString t = arg.mid(11);
            range = t.toInt(&ok);
            if (!ok || range <= 0) {
                fprintf(stderr, "Invalid --range (%s), must be positive integer value\n",
                        qPrintable(arg.mid(12)));
                return 1;
            }
            if (verbose)
                qDebug() << "range:" << range;
        } else if (!image1) {
            image1 = Image::load(arg);
            if (!image1) {
                fprintf(stderr, "Failed to decode %s\n", qPrintable(arg));
                return 1;
            }
        } else if (!image2) {
            image2 = Image::load(arg);
            if (!image2) {
                fprintf(stderr, "Failed to decode %s\n", qPrintable(arg));
                return 1;
            }
        } else {
            usage(stderr);
            fprintf(stderr, "Too many args\n");
            return 1;
        }
    }
    if (!image2) {
        usage(stderr);
        fprintf(stderr, "Not enough args\n");
        return 1;
    }

    if (image1->size() != image2->size()) {
        fprintf(stderr, "Images have different sizes: %dx%d vs %dx%d\n",
                image1->width(), image1->height(),
                image2->width(), image2->height());
        return 1;
    }

    auto chunkIndexes = [range](int count, int idx) {
        QVector<int> indexes;
        const int y = (idx / count);
        const int x = idx % count;
        auto add = [&](int xadd, int yadd) {
            const int xx = x + xadd;
            if (xx < 0 || xx >= count)
                return;
            const int yy = y + yadd;
            if (yy < 0 || yy >= count)
                return;
            indexes.push_back((yy * count) + xx);
        };
        add(0, 0);
        for (int y=-range; y<=range; ++y) {
            for (int x=-range; x<=range; ++x) {
                if (x || y)
                    add(x, y);
            }
        }

        return indexes;
    };

    // qDebug() << chunkIndexes(10, 0);
    // return 0;
    QVector<std::pair<Chunk, Chunk> > matches;
    QRegion used;
    int count = 1;
    QPainter p;
    if (dumpImages) {
        dump = QImage(image1->size(), QImage::Format_ARGB32_Premultiplied);
        dump.fill(qRgba(255, 255, 255, 255));
        p.begin(&dump);
        QFont f;
        f.setPixelSize(8);
        p.setFont(f);
        p.setPen(Qt::black);
    }
    QMap<QPoint, QString> texts;
    while (true) {
        const QVector<Chunk> chunks1 = image1->chunks(count, used);
        if (chunks1.isEmpty())
            break;
        const QVector<Chunk> chunks2 = image2->chunks(count);
        for (int i=0; i<chunks1.size(); ++i) {
            const Chunk &chunk = chunks1.at(i);
            if (chunk.isNull())
                continue;
            Q_ASSERT(chunk.width() >= minSize && chunk.height() >= minSize);

            for (int idx : chunkIndexes(count, i)) {
                const Chunk &otherChunk = chunks2.at(idx);
                if (verbose >= 2) {
                    qDebug() << "comparing chunks" << chunk << otherChunk;
                }

                if (otherChunk.size() == chunk.size() && chunk == otherChunk) {
                    used |= chunk.rect();
                    matches.push_back(std::make_pair(chunk, otherChunk));
                    if (dumpImages) {
                        if (chunk.rect() == otherChunk.rect()) {
                            p.fillRect(chunk.rect(), Qt::green);
                            p.drawRect(chunk.rect());
                            // p.drawText(chunk.rect().topLeft() + QPoint(2, 10),
                            //            toString(chunk.rect()) + "\ndid not move");
                        } else {
                            p.fillRect(chunk.rect(), Qt::yellow);
                            p.drawRect(chunk.rect());
                            texts[chunk.rect().topLeft() + QPoint(2, 10)] = toString(otherChunk.rect()) + " moved to " + toString(chunk.rect());
                        }
                    }
                    break;
                }
            }
        }

        ++count;
    }
    if (dumpImages) {
        p.setPen(Qt::blue);
        // qDebug() << texts;
        for (QMap<QPoint, QString>::const_iterator it = texts.begin(); it != texts.end(); ++it) {
            p.drawText(it.key(), it.value());
        }
    }
    if (!matches.isEmpty()) {
        if (!nojoin)
            joinChunks(matches);
        int i = 0;
        for (const auto &match : matches) {
            if (verbose) {
                QString str;
                QDebug dbg(&str);
                dbg << "Match" << i << match.first.rect();
                if (match.first.rect() == match.second.rect()) {
                    dbg << "SAME";
                } else {
                    dbg << "FOUND AT" << match.second.rect();
                }
                fprintf(stderr, "%s\n", qPrintable(str));
            }
            if (dumpImages) {
                char buf[1024];
                snprintf(buf, sizeof(buf), "/tmp/img-sub_%04d_%s_a%s.png", i, toString(match.first.rect()).constData(),
                         match.first.flags() & Chunk::AllTransparent ? "_alpha" : "");
                match.first.save(buf);
                if (verbose)
                    fprintf(stderr, "Dumped %s\n", buf);
                snprintf(buf, sizeof(buf), "/tmp/img-sub_%04d_%s_b%s.png", i, toString(match.second.rect()).constData(),
                         match.second.flags() & Chunk::AllTransparent ? "_alpha" : "");
                match.second.save(buf);
                if (verbose)
                    fprintf(stderr, "Dumped %s\n", buf);
            }
            if (match.first.rect() != match.second.rect()) {
                if (!same) {
                    printf("%s %s\n", toString(match.first.rect()).constData(), toString(match.second.rect()).constData());
                }
            } else if (same) {
                printf("%s\n", toString(match.first.rect()).constData());
            }
            ++i;
        }
        if (!same) {
            QRegion all;
            all |= image1->rect();
            all -= used;
            for (const QRect &rect : all.rects()) {
                printf("%s\n", toString(rect).constData());
            }
        }
    } else if (!same) {
        printf("%s\n", toString(image1->rect()).constData());
    }
    if (dumpImages) {
        p.end();
        dump.save("/tmp/img-sub.png");
    }

    return 0;
}
