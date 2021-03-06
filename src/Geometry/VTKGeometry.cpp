#include "VTKGeometry.h"
#include "math.h"
#include "vtkTriangleFilter.h"
#include <vtkDelaunay2D.h>
#include <vtkPolygon.h>
#include <vtkPoints.h>
#include <vtkKdTreePointLocator.h>
#include <vtkDataSet.h>
#include "vtkPolyDataConnectivityFilter.h"
#include "vtkClipPolyData.h"
#include "vtkPlanes.h"
#include "vtkPlane.h"
#include "vtkFeatureEdges.h"
#include "vtkStripper.h"
#include "vtkCleanPolyData.h"
#include "vtkCellLocator.h"
#include "vtkGenericCell.h"
#include "vtkMatrix4x4.h"
#include "vtkPointsProjectedHull.h"
#include "QDebug"
#include "OffsetLoop.h"
#include "Vector3.h"
#include "vtkTransform.h"
#include "vtkTransformFilter.h"
#include "vtkSTLWriter.h"
#include <QTime>

VTKGeometry::VTKGeometry()
{

}
/**
 * @brief VTKGeometry::pointMappingPlane 点按照方向投影到平面
 * @param p 需要投影点
 * @param mapNormal 投影方向
 * @param origin 平面上一点
 * @param normal 平面法相
 * @param outP 交点
 */
void VTKGeometry::pointMappingPlane(Point3D p, Normal mapNormal, Point3D origin, Normal normal, Point3D& outP)
{
    vtkSmartPointer<vtkPlane> plane =
        vtkSmartPointer<vtkPlane>::New();
    plane->SetOrigin(origin.x, origin.y, origin.z);
    plane->SetNormal(normal.x, normal.y, normal.z);

    mapNormal.Normalize();

    double p2[3] = {p.x + mapNormal.x*1, p.y + mapNormal.y*1, p.z + mapNormal.z*1};
    double t = 0;
    double pp[3] = {p.x, p.y, p.z};
    double resP[3];
    plane->IntersectWithLine(pp, p2, t, resP);
    outP.x = resP[0];
    outP.y = resP[1];
    outP.z = resP[2];
}
/**
 * @brief VTKGeometry::vec3dot 向量相乘
 * @param a
 * @param b
 * @return 返回正数表示向量夹角小于90度 返回负数表示向量夹角大于90度
 */
float VTKGeometry::vec3dot(Normal a, Normal b)
{
    return (a.x*b.x + a.y*b.y + a.z*b.z);
}
/**
 * @brief VTKGeometry::getAngleDegOf2Vec 向量角度计算
 * @param v1
 * @param v2
 * @return 返回0~180
 */
double VTKGeometry::getAngleDegOf2Vec(Normal v1, Normal v2)
{
    v1.Normalize();
    v2.Normalize();
    float radians = vec3dot(v1, v2);
    if(radians <= -1.0)
    {
        return 180;
    }
    if(radians >= 1.0)
    {
        return 0;
    }
    double rad = acos(radians);

    const float kPi = 3.14159265f;
    const float k180OverPi = 180.0f / kPi;

    return rad * k180OverPi;
}
/**
 * @brief VTKGeometry::cross_Product 向量叉乘
 * @param a
 * @param b
 * @param out
 */
void VTKGeometry::cross_Product(double a[], double b[], double out[])
{
    out[0] = a[1]*b[2] - a[2]*b[1];
    out[1] = a[2]*b[0] - a[0]*b[2];
    out[2] = a[0]*b[1] - a[1]*b[0];
}

void VTKGeometry::cross_Product(const Normal &a, const Normal &b, Normal &out)
{
    out.x = a.y*b.z - a.z*b.y;
    out.y = a.z*b.x - a.x*b.z;
    out.z = a.x*b.y - a.y*b.x;
}
/**
 * @brief VTKGeometry::calIntersectPolydata 线段与模型相交
 * @param p1
 * @param p2
 * @param polydata
 * @param tree 输入tree,能够提高计算速度
 * @return 交点
 */
std::vector<Point3D> VTKGeometry::calIntersectPolydata(Point3D p1, Point3D p2, vtkPolyData *polydata, VSP<vtkOBBTree> tree)
{
    double lineP0[3] = {p1.x, p1.y, p1.z};
    double lineP1[3] = {p2.x, p2.y, p2.z};
    vtkSmartPointer<vtkPoints> intersectPoints =
      vtkSmartPointer<vtkPoints>::New();

    vtkSmartPointer<vtkIdList> intersectCells =
      vtkSmartPointer<vtkIdList>::New();
    if(tree == NULL)
        tree = vtkSmartPointer<vtkOBBTree>::New();
    tree->SetDataSet(polydata);
    tree->BuildLocator();
    double tol = 1.e-8;
    tree->SetTolerance(tol);
    tree->IntersectWithLine(lineP0, lineP1,
                            intersectPoints,
                            intersectCells);
    std::vector<Point3D> points;
    for(int i = 0; i < intersectPoints->GetNumberOfPoints(); i++)
    {
        double* p = intersectPoints->GetPoint(i);
        points.push_back(Point3D(p[0], p[1], p[2]));
    }
    return points;
}
/**
 * @brief VTKGeometry::calIntersectPolydataSingle 射线与模型相交
 * @param p
 * @param n
 * @param polydata
 * @param tree
 * @return 返回距离p最近的一个交点
 */
Point3D VTKGeometry::calIntersectPolydataSingle(Point3D p, Normal n, vtkPolyData* polydata, VSP<vtkOBBTree> tree)
{
    Point3D outPut(0, 0, 0);
    Point3D p1 = p;
    Point3D p2 = p;
    p1.move(&n, -1000);
    p2.move(&n, 1000);
    std::vector<Point3D> points = calIntersectPolydata(p1, p2, polydata, tree);
    double dist = 100000000;
    for(int i = 0; i < points.size(); i++)
    {
        Point3D pp = points[i];
        double d = point3Distance(pp, p1);
        if(dist > d)
        {
            dist = d;
            outPut = pp;
        }
    }
    return outPut;
}
/**
 * @brief VTKGeometry::transformationObjectData 按照矩阵进行变化模型
 * @param m 大小16
 * @param object
 */
void VTKGeometry::transformationObjectData(double m[], vtkPolyData *object)
{
    vtkPoints* points = object->GetPoints();
    for(int i = 0; i < object->GetNumberOfPoints(); i++)
    {
        double* p = points->GetPoint(i);
        double v1 = p[0] * m[0] +
                    p[1] * m[4] +
                    p[2] * m[8];
        double v2 = p[0] * m[1] +
                    p[1] * m[5] +
                    p[2] * m[9];
        double v3 = p[0] * m[2] +
                    p[1] * m[6] +
                    p[2] * m[10];
        p[0] = v1;
        p[1] = v2;
        p[2] = v3;
        points->SetPoint(i, p);
    }
    object->SetPoints(points);
    object->Modified();
}
/**
 * @brief VTKGeometry::transformationCurveData 矩阵与离散点相乘
 * @param m
 * @param fittingPoints
 */
void VTKGeometry::transformationCurveData(double m[], std::vector<Point3D> &fittingPoints)
{
    for(unsigned int i = 0; i < fittingPoints.size(); i++)
    {
        Point3D &p = fittingPoints[i];
        double v1 = p.x * m[0] +
                    p.y * m[4] +
                    p.z * m[8];
        double v2 = p.x * m[1] +
                    p.y * m[5] +
                    p.z * m[9];
        double v3 = p.x * m[2] +
                    p.y * m[6] +
                    p.z * m[10];
        p.x = v1;
        p.y = v2;
        p.z = v3;
    }
}
/**
 * @brief VTKGeometry::transformationNormal 法相按照矩阵进行变换
 * @param m
 * @param n
 */
void VTKGeometry::transformationNormal(double m[], Normal &n)
{
    // 变换方向
    Point3D p1(0, 0, 0);
    Point3D p2 = p1;
    p2.move(&n, 10);

    std::vector<Point3D> pps;
    pps.push_back(p1);
    pps.push_back(p2);
    transformationCurveData(m, pps);
    p1 = pps[0];
    p2 = pps[1];
    n = p2 - p1;
    n.Normalize();
}

void VTKGeometry::transformationObjectData(vtkMatrix4x4 *m, vtkPolyData *object)
{
//    vtkSmartPointer <vtkTransformFilter> transformFilter = vtkSmartPointer<vtkTransformFilter>::New();
//    vtkSmartPointer <vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
//    transform->SetMatrix(m);
//    transformFilter->SetTransform(transform);
//    transformFilter->SetInputData(object);
//    transformFilter->Update();
//    object->DeepCopy(transformFilter->GetOutput());

    vtkSmartPointer <vtkPoints> newPoints = vtkSmartPointer<vtkPoints>::New();
    for(int i = 0; i < object->GetNumberOfPoints(); i++)
    {
        double* p = object->GetPoint(i);
        double pp[4] = {p[0], p[1], p[2], 1};
        m->MultiplyPoint(pp, pp);
        p[0] = pp[0];
        p[1] = pp[1];
        p[2] = pp[2];
        newPoints->InsertPoint(i, p);
    }
    object->SetPoints(newPoints);
}

void VTKGeometry::transformationCurveData(vtkMatrix4x4 *m, std::vector<Point3D> &fittingPoints)
{
    for(int i = 0; i < fittingPoints.size(); i++)
    {
        Point3D& p = fittingPoints[i];
        double pp[4] = {p.x, p.y, p.z, 1};
        m->MultiplyPoint(pp, pp);
        p.x = pp[0];
        p.y = pp[1];
        p.z = pp[2];
    }
}

void VTKGeometry::transformationNormal(vtkMatrix4x4 *m, Normal &n)
{
    // 变换方向
    Point3D p1(0, 0, 0);
    Point3D p2 = p1;
    p2.move(&n, 10);

    std::vector<Point3D> pps;
    pps.push_back(p1);
    pps.push_back(p2);
    transformationCurveData(m, pps);
    p1 = pps[0];
    p2 = pps[1];
    n = p2 - p1;
    n.Normalize();
}
/**
 * @brief VTKGeometry::moveObject 按照向量大小移动模型
 * @param polydata
 * @param n
 */
void VTKGeometry::moveObject(vtkPolyData *polydata, Normal n)
{
    vtkSmartPointer <vtkPoints> newPoints = vtkSmartPointer<vtkPoints>::New();
    for(int i = 0; i < polydata->GetNumberOfPoints(); i++)
    {
        double* p = polydata->GetPoint(i);
        newPoints->InsertPoint(i, p[0] + n.x, p[1] + n.y, p[2] + n.z);
    }
    polydata->SetPoints(newPoints);

}

void VTKGeometry::movePoints(std::vector<Point3D> &points, Normal n)
{
    for(int i = 0; i < points.size(); i++)
    {
        Point3D& p = points[i];
        p = p + n;
    }
}
/**
 * @brief VTKGeometry::calBounds 计算包围盒
 * @param object
 * @param box
 */
void VTKGeometry::calBounds(vtkPolyData *object, double *box)
{
    box[0] = 1000000000;
    box[1] = -100000000;
    box[2] = 1000000000;
    box[3] = -100000000;
    box[4] = 1000000000;
    box[5] = -100000000;
    vtkPoints* points = object->GetPoints();
    for(int i = 0; i < object->GetNumberOfPoints(); i++)
    {
        double* p = points->GetPoint(i);
        if(box[0] > p[0])
        {
            box[0] = p[0];
        }
        if(box[1] < p[0])
        {
            box[1] = p[0];
        }
        if(box[2] > p[1])
        {
            box[2] = p[1];
        }
        if(box[3] < p[1])
        {
            box[3] = p[1];
        }
        if(box[4] > p[2])
        {
            box[4] = p[2];
        }
        if(box[5] < p[2])
        {
            box[5] = p[2];
        }
    }
}
/**
 * @brief VTKGeometry::findAllBorder 查找模型所有边
 * @param object
 * @param cleanPolyData 存储边的数据结构
 */
void VTKGeometry::findAllBorder(vtkPolyData *object, vtkCleanPolyData* cleanPolyData)
{
    vtkSmartPointer<vtkFeatureEdges> boundaryEdges =
            vtkSmartPointer<vtkFeatureEdges>::New();
    boundaryEdges->SetInputData(object);
    boundaryEdges->BoundaryEdgesOn();
    boundaryEdges->FeatureEdgesOff();
    boundaryEdges->ManifoldEdgesOff();
    boundaryEdges->NonManifoldEdgesOff();
    boundaryEdges->Update();

    // 排序
    vtkSmartPointer<vtkStripper> stripper =
            vtkSmartPointer<vtkStripper>::New();
    stripper->SetInputData(boundaryEdges->GetOutput());
    stripper->JoinContiguousSegmentsOn();
    stripper->Update();

    cleanPolyData->SetInputData(stripper->GetOutput());
    cleanPolyData->Update();
}
/**
 * @brief VTKGeometry::findNearestBorder 返回距离P点最近的一条边
 * @param cleanPolyData
 * @param p
 * @return
 */
std::vector<Point3D> VTKGeometry::findNearestBorder(vtkCleanPolyData* cleanPolyData, Point3D p)
{
    std::vector<Point3D> outputPoints;
    // 查找最近的cell
    auto cellLocator = vtkSmartPointer<vtkCellLocator>::New();
    cellLocator->SetDataSet(cleanPolyData->GetOutput());
    cellLocator->BuildLocator();

    double testPoint[3] = {p.x, p.y, p.z};

    //Find the closest points to TestPoint
    auto assistCell = vtkSmartPointer<vtkGenericCell>::New();
    double closestPoint[3];//the coordinates of the closest point will be returned here
    double closestPointDist2; //the squared distance to the closest point will be returned here
    vtkIdType cellId; //the cell id of the cell containing the closest point will be returned here
    int subId;
    cellLocator->FindClosestPoint(testPoint, closestPoint, assistCell, cellId, subId, closestPointDist2);

    vtkCell* cell = cleanPolyData->GetOutput()->GetCell(cellId);

    for(int i = 0; i < cell->GetPoints()->GetNumberOfPoints(); i++)
    {
        double* ver = cell->GetPoints()->GetPoint(i);
        outputPoints.push_back(Point3D(ver[0], ver[1], ver[2]));
    }
    return outputPoints;
}
/**
 * @brief VTKGeometry::rotatePoint 旋转点集
 * @param axis 旋转轴
 * @param angle 旋转角度
 * @param center 旋转中心
 * @param inputPoints 输入和输出
 */
void VTKGeometry::rotatePoint(Normal axis, double angle, Point3D center, std::vector<Point3D> &inputPoints)
{
    custom::matrix  result;
    float A[3] = {(float)center.x, (float)center.y, (float)center.z};
    float dir[3] = {(float)axis.x, (float)axis.y, (float)axis.z};
    GetRoteTransMatrix(A,dir, angle*PI/180, result);
    //
    custom::matrix  mpt(inputPoints.size(),4);
    for(int i = 0; i < inputPoints.size(); i++)
    {
        Point3D& p = inputPoints[i];
        mpt.setValue(i,0,p.x);
        mpt.setValue(i,1,p.y);
        mpt.setValue(i,2,p.z);
        mpt.setValue(i,3,1);
    }

    custom::matrix  mresult;
    mresult = mpt*result;

    for(int i = 0; i < inputPoints.size(); i++)
    {
        Point3D& p = inputPoints[i];
        p.x = mresult.getValue(i,0);
        p.y = mresult.getValue(i,1);
        p.z = mresult.getValue(i,2);
    }
}

void VTKGeometry::getRotateMatrix(Normal axis, double angle, Point3D center, vtkMatrix4x4 *outputM)
{
    custom::matrix  result;
    float A[3] = {(float)center.x, (float)center.y, (float)center.z};
    float dir[3] = {(float)axis.x, (float)axis.y, (float)axis.z};
    GetRoteTransMatrix(A,dir, angle*PI/180, result);
    outputM->SetElement(0, 0, result.getValue(0, 0));
    outputM->SetElement(0, 1, result.getValue(0, 1));
    outputM->SetElement(0, 2, result.getValue(0, 2));
    outputM->SetElement(0, 3, result.getValue(0, 3));

    outputM->SetElement(1, 0, result.getValue(1, 0));
    outputM->SetElement(1, 1, result.getValue(1, 1));
    outputM->SetElement(1, 2, result.getValue(1, 2));
    outputM->SetElement(1, 3, result.getValue(1, 3));

    outputM->SetElement(2, 0, result.getValue(2, 0));
    outputM->SetElement(2, 1, result.getValue(2, 1));
    outputM->SetElement(2, 2, result.getValue(2, 2));
    outputM->SetElement(2, 3, result.getValue(2, 3));

    outputM->SetElement(3, 0, result.getValue(3, 0));
    outputM->SetElement(3, 1, result.getValue(3, 1));
    outputM->SetElement(3, 2, result.getValue(3, 2));
    outputM->SetElement(3, 3, result.getValue(3, 3));
//    vtkSmartPointer<vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
//    transform->Translate(-center.x, -center.y, -center.z);
//    transform->RotateWXYZ(angle, axis.x, axis.y, axis.z);
//    transform->Translate(center.x, center.y, center.z);
//    transform->Update();
//    outputM->DeepCopy(transform->GetMatrix());


//    vtkSmartPointer<vtkTransform> transform1 = vtkSmartPointer<vtkTransform>::New();
//    vtkSmartPointer<vtkTransform> transform2 = vtkSmartPointer<vtkTransform>::New();
//    vtkSmartPointer<vtkTransform> transform3 = vtkSmartPointer<vtkTransform>::New();
//    transform1->Translate(-center.x, -center.y, -center.z);
//    transform2->RotateWXYZ(angle, axis.x, axis.y, axis.z);
//    transform3->Translate(center.x, center.y, center.z);
//    transform1->Update();
//    transform2->Update();
//    transform3->Update();
//    vtkSmartPointer<vtkMatrix4x4> m1 = vtkSmartPointer<vtkMatrix4x4>::New();
//    vtkMatrix4x4::Multiply4x4(transform1->GetMatrix(), transform2->GetMatrix(), m1);
//    vtkMatrix4x4::Multiply4x4(m1, transform3->GetMatrix(), outputM);
}
/**
 * @brief VTKGeometry::GetRoteTransMatrix 计算旋转矩阵 VTK有同样方法，此方法非VTK
 * @param A 旋转中心
 * @param dir 旋转轴
 * @param roteangle 角度
 * @param result
 */
void VTKGeometry::GetRoteTransMatrix(float A[], float dir[], float roteangle, custom::matrix &result)
{
    int i = 0;
    float Rz[][4] = {
        cos(roteangle),sin(roteangle),0,0,
        -sin(roteangle),cos(roteangle),0,0,
        0,0,1,0,
        0,0,0,1,
    };

    custom::matrix  mRz(4,4);
    for(i = 0; i < 4; i++)
        mRz.setrowVal(i,Rz[i]);

    if(roteangle == 0 || roteangle == PI)
    {
        result = mRz;
        return;
    }
    float Ta[][4] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        -A[0],-A[1],-A[2],1,
    };

    float invTa[][4] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        A[0],A[1],A[2],1,
    };

    custom::matrix  mTa(4,4);
    custom::matrix  minvTa(4,4);

    for(i = 0; i < 4; i++)
    {
        mTa.setrowVal(i,Ta[i]);
        minvTa.setrowVal(i,invTa[i]);
    }

    float v = sqrt(dir[1]*dir[1] + dir[2]*dir[2]);
    float cosalpha;
    float sinalpha;
    if(v != 0)
    {
        cosalpha = dir[2]/v;
        sinalpha = dir[1]/v;
    }
    else
    {
        cosalpha = 1;
        sinalpha = 0;
    }
    float Rx[][4] = {
        1,0,0,0,
        0,cosalpha,sinalpha,0,
        0,-sinalpha,cosalpha,0,
        0,0,0,1,
    };
    float invRx[][4] = {
        1,0,0,0,
        0,cosalpha,-sinalpha,0,
        0,sinalpha,cosalpha,0,
        0,0,0,1,
    };
    custom::matrix  mRx(4,4);
    custom::matrix  minvRx(4,4);

    for(i = 0; i < 4; i++)
    {
        mRx.setrowVal(i,Rx[i]);
        minvRx.setrowVal(i,invRx[i]);
    }

    float u = sqrt(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
    float cosbeta = v/u;
    float sinbeta = -dir[0]/u;
    float Ry[][4] = {
        cosbeta,0,-sinbeta,0,
        0,1,0,0,
        sinbeta,0,cosbeta,0,
        0,0,0,1,
    };
    float invRy[][4] = {
        cosbeta,0,sinbeta,0,
        0,1,0,0,
        -sinbeta,0,cosbeta,0,
        0,0,0,1,
    };

    custom::matrix  mRy(4,4);
    custom::matrix  minvRy(4,4);

    for(i = 0; i < 4; i++)
    {
        mRy.setrowVal(i,Ry[i]);
        minvRy.setrowVal(i,invRy[i]);
    }

    result = mTa*mRx*mRy*mRz*minvRy*minvRx*minvTa;
}
/**
 * @brief VTKGeometry::getLoopMinDist 查找p距离loop点集中最近的点
 * @param p
 * @param loop
 * @param kdTree 添加kdtree能够加快计算
 * @return
 */
int VTKGeometry::getLoopMinDist(Point3D p, std::vector<Point3D> &loop, kdtree *kdTree)
{
    bool isFree = false;
    if(kdTree == NULL)
    {
        isFree = true;
        kdTree = kd_create(3);
        for(unsigned int i = 0; i < loop.size(); i++)
        {
            Point3D& p = loop[i];
            kd_insert3(kdTree, p.x, p.y, p.z, (void*)i);
        }
    }
    kdres *res = kd_nearest3(kdTree, p.x, p.y, p.z);
    unsigned int pi = (unsigned int)kd_res_item_data(res);
    kd_res_free(res);
    if(isFree)
    {
        kd_free(kdTree);
    }

    return pi;
}
/**
 * @brief VTKGeometry::calPolyData2DHull_Z 计算XY平面的模型凸包
 * @param object
 * @return
 */
std::vector<Point3D> VTKGeometry::calPolyData2DHull_Z(vtkPolyData *object)
{
    std::vector<Point3D> output;
    vtkSmartPointer<vtkPointsProjectedHull> hull = vtkSmartPointer<vtkPointsProjectedHull>::New();
    hull->DeepCopy(object->GetPoints());
    int zSize = hull->GetSizeCCWHullZ();

    double *pts=new double[zSize*2];
    hull->GetCCWHullZ(pts,zSize);

    vtkSmartPointer<vtkPoints> zHullPoints=
            vtkSmartPointer<vtkPoints>::New();
    for(int i=0;i<zSize;++i)
    {
        double xval=pts[2*i];
        double yval=pts[2*i+1];
        Point3D p(xval, yval, 0);
        output.push_back(p);
    }
    return output;
}
/**
 * @brief VTKGeometry::calPolyData2DHull_Y 计算XZ平面的模型凸包
 * @param object
 * @return
 */
std::vector<Point3D> VTKGeometry::calPolyData2DHull_Y(vtkPolyData *object)
{
    std::vector<Point3D> output;
    vtkSmartPointer<vtkPointsProjectedHull> hull = vtkSmartPointer<vtkPointsProjectedHull>::New();
    hull->DeepCopy(object->GetPoints());
    int zSize = hull->GetSizeCCWHullY();

    double *pts=new double[zSize*2];
    hull->GetCCWHullY(pts,zSize);

    vtkSmartPointer<vtkPoints> zHullPoints=
            vtkSmartPointer<vtkPoints>::New();
    for(int i=0;i<zSize;++i)
    {
        double xval=pts[2*i];
        double yval=pts[2*i+1];
        Point3D p(xval, yval, 0);
        output.push_back(p);
    }
    return output;
}
/**
 * @brief VTKGeometry::calPolyData2DHull_X 计算YZ平面模型凸包
 * @param object
 * @return
 */
std::vector<Point3D> VTKGeometry::calPolyData2DHull_X(vtkPolyData *object)
{
    std::vector<Point3D> output;
    vtkSmartPointer<vtkPointsProjectedHull> hull = vtkSmartPointer<vtkPointsProjectedHull>::New();
    hull->DeepCopy(object->GetPoints());
    int zSize = hull->GetSizeCCWHullX();

    double *pts=new double[zSize*2];
    hull->GetCCWHullX(pts,zSize);

    vtkSmartPointer<vtkPoints> zHullPoints=
            vtkSmartPointer<vtkPoints>::New();
    for(int i=0;i<zSize;++i)
    {
        double xval=pts[2*i];
        double yval=pts[2*i+1];
        Point3D p(xval, yval, 0);
        output.push_back(p);
    }
    return output;
}
/**
 * @brief VTKGeometry::fittedCurve 曲线加密
 * @param curve 输入曲线
 * @param newCurve // 输出曲线
 * @param dist 加密距离
 * @param v 首末点是否加密
 */
void VTKGeometry::fittedCurve(std::vector<Point3D> curve, std::vector<Point3D> &newCurve, double dist, bool v)
{
//    LOGFUNC();
    newCurve.clear();
    if(0 == curve.size())
    {
        return ;
    }
    if(v == true)
    {
        if(curve[0] != curve[curve.size() - 1])
        {
            curve.push_back(curve[0]);
        }
    }
    for(size_t i = 0; i < curve.size() - 1; i++)
    {
        Point3D theP = curve[i];
        Point3D& nextPoint = curve[i + 1];
        Normal moveNormal = nextPoint - theP;
        newCurve.push_back(theP);
        double d = point3Distance(theP, nextPoint);
        if(d > dist)
        {
            int count = ceil(d/dist);
            double newDist = d/count;
            for(int k = 0; k < count - 1; k++)
            {
                theP.move(&moveNormal, newDist);
                newCurve.push_back(theP);
            }
        }
    }
    newCurve.push_back(curve[curve.size() - 1]);
}
/**
 * @brief VTKGeometry::sparsePoints 稀疏曲线
 * @param points 输入
 * @param newPoints 输出
 * @param dist 稀疏距离
 */
void VTKGeometry::sparsePoints(std::vector<Point3D> points, std::vector<Point3D> &newPoints, double dist)
{
//    LOGFUNC();
    if(0 == points.size())
    {
        return ;
    }
    newPoints.clear();
    for(size_t i = 0; i < points.size() - 1; i++)
    {
        Point3D currentPoint = points[i];
        newPoints.push_back(currentPoint);
        double pathDist = 0;
        for(size_t j = i + 1; j < points.size(); j++)
        {
            Point3D& nextPoint = points[j];
            double d = point3Distance(currentPoint, nextPoint);
            if(pathDist < dist)
            {
                i = j - 1;
                currentPoint = nextPoint;
                pathDist = pathDist + d;
            } else {
                break;
            }
        }
    }
    newPoints.push_back(points.at(points.size() - 1));
}

/**
 * @brief VTKGeometry::offsetLoop 曲线偏执 输入曲线会按照d大小，向内进行偏执，直到偏执到一点为止
 * @param currentLoop
 * @param d 必须 > 0
 * @param outLoops
 */
void VTKGeometry::offsetLoop(std::vector<Point3D> currentLoop, double d, std::vector<std::vector<Point3D> > &outLoops)
{
    outLoops.push_back(currentLoop);
    std::vector<std::vector<Point3D>> tempLoops;
    while(1)
    {
        std::vector<std::vector<Point3D> > loops;
        OffsetLoop::offset(currentLoop, loops, -d);
        if(loops.size() == 0)
        {
            break;
        }
        currentLoop = loops[0];


        fittedCurve(currentLoop, currentLoop, d);
        sparsePoints(currentLoop, currentLoop, d);


        outLoops.push_back(currentLoop);
        for(size_t j = 1; j < loops.size(); j++)
        {
            tempLoops.push_back(loops[j]);
        }
    }

    for(size_t i = 0; i < tempLoops.size(); i++)
    {
        currentLoop = tempLoops[i];
        offsetLoop(currentLoop, d, outLoops);
    }
}
/**
 * @brief VTKGeometry::genDelaunay 三角剖分函数，此函数2Ddelaunay剖分，没有边界限制
 * @param allpoints
 * @param polydata
 */
void VTKGeometry::genDelaunay(std::vector<Point3D> allpoints, vtkPolyData *polydata)
{
    vtkSmartPointer<vtkPoints> points =
            vtkSmartPointer<vtkPoints>::New();
    for(size_t i = 0; i < allpoints.size(); i++)
    {
        Point3D& p = allpoints[i];
        points->InsertNextPoint(p.x,p.y,p.z);
    }
    vtkSmartPointer<vtkPolyData> polys =
            vtkSmartPointer<vtkPolyData>::New();
    polys->SetPoints(points);

    vtkSmartPointer<vtkDelaunay2D> triangulator =
            vtkSmartPointer<vtkDelaunay2D>::New();

    triangulator->SetInputData(polys);
    triangulator->Update();

    polydata->DeepCopy(triangulator->GetOutput());
}
/**
 * @brief VTKGeometry::outputPtList 将离散点保存到本地文件中
 * @param pts
 * @param fileName
 */
void VTKGeometry::outputPtList(const std::vector<Point3D> &pts, std::string fileName)
{
    std::ofstream outf(fileName, std::ios::trunc);
    for(unsigned int k = 0; k < pts.size(); ++k)
    {
        outf << pts[k].x << " " << pts[k].y << " " << pts[k].z << std::endl;
    }
    outf.close();
}

///  @brief 计算一个向量的垂直向量
Normal VTKGeometry::unitOrthoVec(Normal srcVec)
{
     /* unless the x and y coords are both close to zero, we can
     * simply take ( -y, x, 0 ) and normalize it.
     */
    Normal orthVec;
    if(!(fabs(srcVec.x) < 0.1 && fabs(srcVec.y) < 0.1))
    {
          orthVec.x = -srcVec.y;
          orthVec.y = srcVec.x;
          orthVec.z = 0;
    }
    /* if both x and y are close to zero, then the vector is close
     * to the z-axis, so it's far from colinear to the x-axis for instance.
     * So we take the crossed product with (1,0,0) and normalize it.
     */
    else
    {
      orthVec.x  = 0;
      orthVec.y  = -srcVec.z;
      orthVec.z  = srcVec.y;
    }
    orthVec.Normalize();
    return orthVec;
}

/**
 * @brief 根据Z轴方向构造一个局部坐标系
 * @param zDir,输入,Z轴
 * @param xDir,输出,x轴
 * @param yDir,输出,y轴
 */
void VTKGeometry::constructCoordinateByZAxle(const Normal &zDir, Normal &xDir, Normal &yDir)
{
    Normal z = zDir;
    z.Normalize();
    xDir = unitOrthoVec(z);
    cross_Product(z, xDir, yDir);
    yDir.Normalize();
}

Point3D VTKGeometry::findPointsMinDist(std::vector<Point3D> points, Point3D inputP)
{
    Point3D outPutPoint(0, 0, 0);
    float dist = 10000000;
    for(int i = 0; i < points.size(); i++)
    {
        Point3D p = points[i];
        float d = point3Distance(p, inputP);
        if(dist > d)
        {
            dist = d;
            outPutPoint = p;
        }
    }
    return outPutPoint;
}

void VTKGeometry::makeTriangles(std::vector<std::vector<Point3D> > points, vtkPolyData *polyData, bool isClose)
{

    vtkSmartPointer<vtkPoints> outPoints = vtkSmartPointer<vtkPoints>::New();
    for(int i = 0; i < points.size(); i++)
    {
        std::vector<Point3D>& rowsPoints = points[i];
        for(int j = 0; j < rowsPoints.size(); j++)
        {
            Point3D p = rowsPoints[j];
            outPoints->InsertNextPoint(p.x, p.y, p.z);
        }

    }

    int rows = points.size();
    int cols = points[0].size();
    auto polys = vtkSmartPointer<vtkCellArray>::New();
    for(int i = 0; i < rows - 1; i++)
    {
        for(int j = 0; j < cols - 1; j++)
        {
            int v1 = i*cols + j;
            int v2 = (i+1)*cols + j+1;
            int v3 = (i+1)*cols + j;

            int vv1 = i*cols + j;
            int vv2 = i*cols + j+1;
            int vv3 = (i+1)*cols + j+1;
            polys->InsertNextCell(3);
            polys->InsertCellPoint(v1);
            polys->InsertCellPoint(v2);
            polys->InsertCellPoint(v3);
            polys->InsertNextCell(3);
            polys->InsertCellPoint(vv1);
            polys->InsertCellPoint(vv2);
            polys->InsertCellPoint(vv3);
        }
    }
    if(isClose)
    {
        int endRow = rows - 1;
        int startRow = 0;
        for(int j = 0; j < cols - 1; j++)
        {
            int v1 = endRow*cols + j;
            int v2 = startRow*cols + j+1;
            int v3 = startRow*cols + j;

            int vv1 = endRow*cols + j;
            int vv2 = endRow*cols + j+1;
            int vv3 = startRow*cols + j+1;
            polys->InsertNextCell(3);
            polys->InsertCellPoint(v1);
            polys->InsertCellPoint(v2);
            polys->InsertCellPoint(v3);
            polys->InsertNextCell(3);
            polys->InsertCellPoint(vv1);
            polys->InsertCellPoint(vv2);
            polys->InsertCellPoint(vv3);
        }
    }
    polyData->SetPoints(outPoints);
    polyData->SetPolys(polys);
    polyData->BuildCells();
    polyData->Modified();

}

QColor VTKGeometry::getRandomColor()
{
    static int colorIndex = 0;
    std::vector<QColor> colors;
    //-----------------预留颜色----------------------//
    colors.push_back(QColor(255, 0, 0));colors.push_back(QColor(0, 255, 0));colors.push_back(QColor(0, 0, 255));colors.push_back(QColor(255, 255, 0));
    colors.push_back(QColor(0, 255, 255));colors.push_back(QColor(255, 0, 255));colors.push_back(QColor(200, 100, 0));colors.push_back(QColor(100, 200, 0));
    colors.push_back(QColor(200, 0, 100));colors.push_back(QColor(0, 200, 100));colors.push_back(QColor(0, 100, 200));colors.push_back(QColor(100, 0, 200));
    colors.push_back(QColor(100, 50, 0));colors.push_back(QColor(50, 100, 0));colors.push_back(QColor(100, 0, 50));colors.push_back(QColor(0, 100, 50));
    colors.push_back(QColor(0, 50, 100));colors.push_back(QColor(50, 0, 100));colors.push_back(QColor(250, 150, 50));colors.push_back(QColor(150, 50, 250));
    colors.push_back(QColor(50, 250, 150));colors.push_back(QColor(180, 100, 10));colors.push_back(QColor(100, 10, 180));colors.push_back(QColor(10, 180, 100));
    QColor color = colors[colorIndex];
    colorIndex++;
    if(colorIndex >= colors.size())
    {
        colorIndex = 0;
    }
    return color;
}


