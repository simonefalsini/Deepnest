#ifndef CONTAINER_DIALOG_H
#define CONTAINER_DIALOG_H

#include <QDialog>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>

/**
 * @brief Dialog for setting container dimensions
 * 
 * Allows user to specify width and height for the nesting container.
 * Used when loading SVG without a container or when modifying container size.
 */
class ContainerDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Parent widget
     */
    explicit ContainerDialog(QWidget* parent = nullptr);

    /**
     * @brief Get container width
     * @return Width in mm
     */
    double getWidth() const { return width_; }

    /**
     * @brief Get container height
     * @return Height in mm
     */
    double getHeight() const { return height_; }

    /**
     * @brief Set initial width value
     * @param width Width in mm
     */
    void setWidth(double width);

    /**
     * @brief Set initial height value
     * @param height Height in mm
     */
    void setHeight(double height);

private slots:
    /**
     * @brief Accept dialog and save values
     */
    void accept() override;

private:
    QDoubleSpinBox* widthSpinBox_;
    QDoubleSpinBox* heightSpinBox_;
    double width_;
    double height_;
};

#endif // CONTAINER_DIALOG_H
