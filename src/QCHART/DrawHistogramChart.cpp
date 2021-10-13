#include "DrawHistogramChart.h"
#include "QHBoxLayout"

DrawHistogramChart::DrawHistogramChart(QWidget *parent) :
    QWidget(parent)
{
    chart = new QChart();
    chartView = new ChartView(chart);
    QHBoxLayout* layout = new QHBoxLayout;
    layout->setMargin(0);
    layout->addWidget(chartView);
    this->setLayout(layout);
}

DrawHistogramChart::~DrawHistogramChart()
{
    delete chart;
    delete chartView;
}
