#include "ContainerDialog.h"
#include <QVBoxLayout>
#include <QLabel>

ContainerDialog::ContainerDialog(QWidget* parent)
    : QDialog(parent)
    , width_(500.0)
    , height_(400.0)
{
    setWindowTitle("Set Container Size");
    setModal(true);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Info label
    QLabel* infoLabel = new QLabel(
        "Define the container (sheet) dimensions for nesting.\n"
        "Parts will be placed within this area."
    );
    infoLabel->setWordWrap(true);
    mainLayout->addWidget(infoLabel);

    // Form layout for inputs
    QFormLayout* formLayout = new QFormLayout();

    // Width input
    widthSpinBox_ = new QDoubleSpinBox();
    widthSpinBox_->setRange(10.0, 100000.0);
    widthSpinBox_->setDecimals(2);
    widthSpinBox_->setSuffix(" mm");
    widthSpinBox_->setValue(width_);
    widthSpinBox_->setSingleStep(10.0);
    formLayout->addRow("Width:", widthSpinBox_);

    // Height input
    heightSpinBox_ = new QDoubleSpinBox();
    heightSpinBox_->setRange(10.0, 100000.0);
    heightSpinBox_->setDecimals(2);
    heightSpinBox_->setSuffix(" mm");
    heightSpinBox_->setValue(height_);
    heightSpinBox_->setSingleStep(10.0);
    formLayout->addRow("Height:", heightSpinBox_);

    mainLayout->addLayout(formLayout);

    // Buttons
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel
    );
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ContainerDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &ContainerDialog::reject);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
    resize(350, 200);
}

void ContainerDialog::setWidth(double width) {
    width_ = width;
    widthSpinBox_->setValue(width);
}

void ContainerDialog::setHeight(double height) {
    height_ = height;
    heightSpinBox_->setValue(height);
}

void ContainerDialog::accept() {
    width_ = widthSpinBox_->value();
    height_ = heightSpinBox_->value();
    QDialog::accept();
}
