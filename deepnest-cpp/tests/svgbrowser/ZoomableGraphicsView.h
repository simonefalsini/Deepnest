#ifndef ZOOMABLEGRAPHICSVIEW_H
#define ZOOMABLEGRAPHICSVIEW_H

#include <QGraphicsView>
#include <QWheelEvent>

class ZoomableGraphicsView : public QGraphicsView {
    Q_OBJECT

public:
    explicit ZoomableGraphicsView(QWidget* parent = nullptr);
    explicit ZoomableGraphicsView(QGraphicsScene* scene, QWidget* parent = nullptr);

    void fitToContent();

protected:
    void wheelEvent(QWheelEvent* event) override;
};

#endif // ZOOMABLEGRAPHICSVIEW_H
