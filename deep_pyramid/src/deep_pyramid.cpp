#include <deep_pyramid.h>
#include <nms.h>
#include <assert.h>
#include <time.h>
#include <string>
#include <algorithm>

#include "rectangle_transform.h"

using namespace cv;
using namespace std;
using namespace caffe;

Rect DeepPyramid::norm5Rect2Original(const Rect& norm5Rect, int level, const Size& imgSize) const
{
    double longSide=std::max(imgSize.height, imgSize.width);
    Size networkOutputSize=net->outputLayerSize();
    double scale=(pow(2, (levelCount-1-level)/2.0)*longSide)/networkOutputSize.width;
    return scaleRect(norm5Rect, scale);
}

//Image Pyramid
//
Size DeepPyramid::embeddedImageSize(const Size& imgSize, const int& i) const
{
    Size networkInputSize=net->inputLayerSize();
    Size newImgSize;
    double scale=1/pow(2,(levelCount-1-i)/2.0);
    double aspectRatio=imgSize.height/(double)imgSize.width;
    if(imgSize.height<=imgSize.width)
    {
        newImgSize.width=networkInputSize.width*scale;
        newImgSize.height=newImgSize.width*aspectRatio;
    }
    else
    {
        newImgSize.height=networkInputSize.height*scale;
        newImgSize.width=newImgSize.height/aspectRatio;
    }

    return newImgSize;
}

void DeepPyramid::constructImagePyramid(const Mat& img, vector<Mat>& imgPyramid) const
{
    Size imgSize(img.cols, img.rows);
    for(int level=0; level<levelCount; level++)
    {
        Mat imgAtLevel(net->inputLayerSize(),CV_8UC3, Scalar::all(0));


        Mat resizedImg;
        Size resizedImgSize=embeddedImageSize(imgSize, level);
        resize(img, resizedImg, resizedImgSize);
        resizedImg.copyTo(imgAtLevel(Rect(Point(0,0),resizedImgSize)));
        imgPyramid.push_back(imgAtLevel);
    }
    cout<<"Create image pyramid..."<<endl;
    cout<<"Status: Success!"<<endl;
}
//
////

void DeepPyramid::constructFeatureMapPyramid(const Mat& img, vector<FeatureMap>& maps) const
{
    vector<Mat> imgPyramid;
    constructImagePyramid(img, imgPyramid);
    for(int i=0; i<levelCount; i++)
    {
        FeatureMap map;
        net->processImage(imgPyramid[i], map);
        map.normalize();
        maps.push_back(map);
    }
}
//
////
void DeepPyramid::detect(const vector<FeatureMap>& maps, vector<BoundingBox>& detectedObjects) const
{
    for(unsigned int i=0;i<rootFilter.size();i++)
        for(unsigned int j=0;j<maps.size();j++)
        {
            std::vector<cv::Rect> detectedRect;
            std::vector<double> confidence;
            rootFilter[i].processFeatureMap(maps[j], detectedRect, confidence, stride);
            for(unsigned int k=0;k<detectedRect.size();k++)
            {
                BoundingBox object;
                object.confidence=confidence[k];
                object.level=j;
                object.norm5Box=detectedRect[k];
                detectedObjects.push_back(object);
            }
        }
}
//
////

//Rectangle
//

void DeepPyramid::calculateOriginalRectangle(vector<BoundingBox>& detectedObjects, const Size& imgSize) const
{
    for(unsigned int i=0;i<detectedObjects.size();i++)
    {
        Rect originalRect=norm5Rect2Original(detectedObjects[i].norm5Box, detectedObjects[i].level, imgSize);
        detectedObjects[i].originalImageBox=originalRect;
    }
}

void DeepPyramid::groupRectangle(vector<BoundingBox>& detectedObjects) const
{
    NMSavg nms;
    nms.processBondingBox(detectedObjects,0.2,0.7);
}

void DeepPyramid::detect(const Mat& img, vector<Rect>& detectedObjects, vector<float>& confidence, bool isBoundingBoxRegressor) const
{
    CV_Assert(img.channels()==3);
    vector<FeatureMap> maps;
    constructFeatureMapPyramid(img, maps);
    cout<<img.cols<<","<<img.rows<<","<<rootFilter.size()<<endl;
    vector<BoundingBox> objects;
    detect(maps, objects);
    cout<<"group rectangle"<<endl;
    calculateOriginalRectangle(objects, Size(img.cols, img.rows));
    groupRectangle(objects);
    if(isBoundingBoxRegressor)
    {
        cout<<"boundbox regressor: TODO"<<endl;
    }
    else
    {
        cout<<"bounding box regressor switch off"<<endl;
    }
    cout<<"Object count:"<<objects.size()<<endl;
    for(unsigned int i=0;i<objects.size();i++)
    {
        detectedObjects.push_back(objects[i].originalImageBox);
        confidence.push_back(objects[i].confidence);
    }
}

DeepPyramid::DeepPyramid(string model_file, string trained_net_file,
                         vector<string> svm_file, vector<Size> svmSize,
                         int _levelCount, int _stride)
{
    net=new NeuralNetwork(model_file, trained_net_file);
    levelCount=_levelCount;
    stride=_stride;
    for(unsigned int i=0;i<svm_file.size();i++)
    {
        CvSVM* classifier=new CvSVM();
        classifier->load(svm_file[i].c_str());
        rootFilter.push_back(RootFilter(svmSize[i], classifier));
    }
}

void DeepPyramid::detect(const Mat &img, vector<BoundingBox> &objects, bool isBoundingBoxRegressor) const
{
    CV_Assert(img.channels()==3);
    vector<FeatureMap> maps;
    constructFeatureMapPyramid(img, maps);
    cout<<"filter"<<endl;
    detect(maps, objects);
    cout<<"group rectangle"<<endl;
    calculateOriginalRectangle(objects, Size(img.cols, img.rows));
    groupRectangle(objects);
    if(isBoundingBoxRegressor)
    {
        cout<<"boundbox regressor: TODO"<<endl;
    }
    else
    {
        cout<<"bounding box regressor switch off"<<endl;
    }
    cout<<"Object count:"<<objects.size()<<endl;
}

DeepPyramid::~DeepPyramid()
{
}
