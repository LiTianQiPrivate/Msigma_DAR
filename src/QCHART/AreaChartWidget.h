#ifndef AREACHARTWIDGET_H
#define AREACHARTWIDGET_H

#include <QWidget>
#include <QtCharts/QChartGlobal>
#include <QtCharts/QChartView>
#include <QtCharts/QPieSeries>
#include <QtCharts/QCandlestickSet>
#include <QtCharts/QPieSlice>
#include <QtCharts/QAbstractBarSeries>
#include <QtCharts/QPercentBarSeries>
#include <QtCharts/QStackedBarSeries>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QLineSeries>
#include <QtCharts/QSplineSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QAreaSeries>
#include <QtCharts/QLegend>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtCore/QRandomGenerator>
#include <QtCharts/QBarCategoryAxis>
#include <QtWidgets/QApplication>
#include <QtCharts/QValueAxis>
#include "ModelBase.h"
#include "MsigmaBase.h"
#include "ChartView.h"
QT_CHARTS_BEGIN_NAMESPACE
class QChartView;
class QChart;
QT_CHARTS_END_NAMESPACE
QT_CHARTS_USE_NAMESPACE


/**
 * @brief The AreaChartWidget class
 * 多曲线面积图
 */

class AreaChartWidget : public QWidget,public MsigmaBase
{
    Q_OBJECT

public:
    AreaChartWidget(QWidget *parent = nullptr, QString className = "AreaChartWidget");
    ~AreaChartWidget();
    void setModelBases(std::vector<ModelBase*> modelObjects,int index);
    void addModelBase(ModelBase* modelObject);
    void clearModelBase();
    void showModelBase(int index);
    void hideModelBase(int index);
    void removeModelBase(int index);
    bool isVisible(int index);
    void on_lineStyleComboBoxActivated(int v,int index){};
private:

    QChart *chart = nullptr;
    std::vector<QLineSeries *> lines;
    std::vector<QAreaSeries*> areaSeriess;
    QAreaSeries *area = nullptr;
    QChartView *chartView = nullptr;


};

#endif // AREACHARTWIDGET_H
