#include "ZoomableGraphicsView.h"
#include <cmath>

ZoomableGraphicsView::ZoomableGraphicsView(QWidget* parent)
    : QGraphicsView(parent)
{
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
}

ZoomableGraphicsView::ZoomableGraphicsView(QGraphicsScene* scene, QWidget* parent)
    : QGraphicsView(scene, parent)
{
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
}

void ZoomableGraphicsView::wheelEvent(QWheelEvent* event)
{
    // Calculate zoom factor
    const double zoomFactor = 1.15;
    double factor;
    
    if (event->angleDelta().y() > 0) {
        factor = zoomFactor;
    } else {
        factor = 1.0 / zoomFactor;
    }

    scale(factor, factor);
    event->accept();
}

void ZoomableGraphicsView::fitToContent()
{
    if (!scene() || scene()->items().isEmpty()) return;
    
    QRectF bounds = scene()->itemsBoundingRect();
    // Add 10% margin
    double margin = std::max(bounds.width(), bounds.height()) * 0.1;
    bounds.adjust(-margin, -margin, margin, margin);
    
    fitInView(bounds, Qt::KeepAspectRatio);
}
