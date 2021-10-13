#ifndef DRAWHISTOGRAMCHART_H
#define DRAWHISTOGRAMCHART_H

#include <QWidget>
#include "ChartView.h"
#include "QBarSeries"
QT_CHARTS_BEGIN_NAMESPACE
class QChartView;
class QChart;
QT_CHARTS_END_NAMESPACE
QT_CHARTS_USE_NAMESPACE
class DrawHistogramChart : public QWidget
{
    Q_OBJECT

public:
    explicit DrawHistogramChart(QWidget *parent = nullptr);
    ~DrawHistogramChart();
public:
    QChart *chart = nullptr;
    ChartView *chartView = nullptr;
    std::vector<QBarSeries *>barseries;

};

#endif // DRAWHISTOGRAMCHART_H
