/**
* This file is part of ORB-SLAM2.
*
* Copyright (C) 2014-2016 Raúl Mur-Artal <raulmur at unizar dot es> (University of Zaragoza)
* For more information see <https://github.com/raulmur/ORB_SLAM2>
*
* ORB-SLAM2 is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* ORB-SLAM2 is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with ORB-SLAM2. If not, see <http://www.gnu.org/licenses/>.
*/

#include "Viewer.h"
#include <pangolin/pangolin.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <mutex>

namespace ORB_SLAM2
{

Viewer::Viewer(System* pSystem, FrameDrawer *pFrameDrawer, MapDrawer *pMapDrawer, Tracking *pTracking, const string &strSettingPath):
    mpSystem(pSystem), mpFrameDrawer(pFrameDrawer),mpMapDrawer(pMapDrawer), mpTracker(pTracking),
    mbFinishRequested(false), mbFinished(true), mbStopped(true), mbStopRequested(false)
{
    cv::FileStorage fSettings(strSettingPath, cv::FileStorage::READ);
    float fps = fSettings["Camera.fps"];
    if(fps<1)
        fps=30;
    mT = 1e3/fps;
    initialized = false;

    mImageWidth = fSettings["Camera.width"];
    mImageHeight = fSettings["Camera.height"];
    if(mImageWidth<1 || mImageHeight<1)
    {
        mImageWidth = 640;
        mImageHeight = 480;
    }

    mViewpointX = fSettings["Viewer.ViewpointX"];
    mViewpointY = fSettings["Viewer.ViewpointY"];
    mViewpointZ = fSettings["Viewer.ViewpointZ"];
    mViewpointF = fSettings["Viewer.ViewpointF"];

}

bool compareMapPointsLex(MapPoint * a, MapPoint *b) {
    cv::Mat mA = a->GetWorldPos();
    cv::Mat mB = b->GetWorldPos();
    // return a > b;
    return  ( mA.at<float>(0) >  mB.at<float>(0)) ||  
            ( mA.at<float>(0) == mB.at<float>(0) && mA.at<float>(1) >  mB.at<float>(1)) ||   
            ( mA.at<float>(0) == mB.at<float>(0) && mA.at<float>(1) == mB.at<float>(1) && mA.at<float>(2) > mB.at<float>(2)) ;
}


void Viewer::FillVectorWithFigure() {
    // for (unsigned int i = 0; i < vpMP.size(); i++)
    // {
    //     if(vpMP[i] > 0) {
    //         cout << "Hay elemento: " << i << " " << vpMP[i]->GetWorldPos();

    //     } else {
    //         cout << "No hay elemento " << i;
    //     }
    // }
    // cout << "\n\n\n";
    // MapPoint vector<MapPoint*>::iterator first= vpMP.begin();
    std::sort(vpMP.begin(), vpMP.begin()+ 2, compareMapPointsLex);
    cv::Mat posA = vpMP[0]->GetWorldPos();
    cv::Mat posB = vpMP[1]->GetWorldPos();
    cv::Mat posC = vpMP[2]->GetWorldPos(); 
    cv::Mat vectorBA = posA - posB;
    cv::Mat vectorBC = posC - posB;

    cv::Mat normal = vectorBC.cross(vectorBA);
    cv::Mat baricenter(3,1,CV_32F);
    baricenter.at<float>(0)=(posA.at<float>(0) + posB.at<float>(0) + posC.at<float>(0))/3.0;
    baricenter.at<float>(1)=(posA.at<float>(1) + posB.at<float>(1) + posC.at<float>(1))/3.0;
    baricenter.at<float>(2)=(posA.at<float>(2) + posB.at<float>(2) + posC.at<float>(2))/3.0;
    cv::Mat posD = baricenter + normal;
    vpMP[3] = new MapPoint(posD);
    vpMP[3]->lockInPicture();
}

void Viewer::Run()
{
    mbFinished = false;
    mbStopped = false;
    int n = 4;
    vpMP = vector<MapPoint*>(n,static_cast<MapPoint*>(NULL));
    pangolin::CreateWindowAndBind("ORB-SLAM2: Map Viewer",1024,768);

    // 3D Mouse handler requires depth testing to be enabled
    glEnable(GL_DEPTH_TEST);

    // Issue specific OpenGl we might need
    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    pangolin::CreatePanel("menu").SetBounds(0.0,1.0,0.0,pangolin::Attach::Pix(175));
    pangolin::Var<bool> menuFollowCamera("menu.Follow Camera",true,true);
    pangolin::Var<bool> menuShowPoints("menu.Show Points",false,false);
    pangolin::Var<bool> menuShowKeyFrames("menu.Show KeyFrames",true,true);
    pangolin::Var<bool> menuShowGraph("menu.Show Graph",false,false);
    pangolin::Var<bool> menuLocalizationMode("menu.Localization Mode",false,false);
    pangolin::Var<bool> menuReset("menu.Reset",false,false);

    // Define Camera Render Object (for view / scene browsing)
    pangolin::OpenGlRenderState s_cam(
                pangolin::ProjectionMatrix(1024,768,mViewpointF,mViewpointF,512,389,0.1,1000),
                pangolin::ModelViewLookAt(mViewpointX,mViewpointY,mViewpointZ, 0,0,0,0.0,-1.0, 0.0)
                );

    // Add named OpenGL viewport to window and provide 3D Handler
    pangolin::View& d_cam = pangolin::CreateDisplay()
            .SetBounds(0.0, 1.0, pangolin::Attach::Pix(175), 1.0, -1024.0f/768.0f)
            .SetHandler(new pangolin::Handler3D(s_cam));

    pangolin::OpenGlMatrix Twc;
    Twc.SetIdentity();

    cv::namedWindow("ORB-SLAM2: Current Frame");

    bool bFollow = true;
    bool bLocalizationMode = false;

    while(1)
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        mpMapDrawer->GetCurrentOpenGLCameraMatrix(Twc);

        if(menuFollowCamera && bFollow)
        {
            s_cam.Follow(Twc);
        }
        else if(menuFollowCamera && !bFollow)
        {
            s_cam.SetModelViewMatrix(pangolin::ModelViewLookAt(mViewpointX,mViewpointY,mViewpointZ, 0,0,0,0.0,-1.0, 0.0));
            s_cam.Follow(Twc);
            bFollow = true;
        }
        else if(!menuFollowCamera && bFollow)
        {
            bFollow = false;
        }

        if(menuLocalizationMode && !bLocalizationMode)
        {
            mpSystem->ActivateLocalizationMode();
            bLocalizationMode = true;
        }
        else if(!menuLocalizationMode && bLocalizationMode)
        {
            mpSystem->DeactivateLocalizationMode();
            bLocalizationMode = false;
        }

        d_cam.Activate(s_cam);
        glClearColor(1.0f,1.0f,1.0f,1.0f);
        mpMapDrawer->DrawCurrentCamera(Twc);
        if(menuShowKeyFrames || menuShowGraph)
            mpMapDrawer->DrawKeyFrames(menuShowKeyFrames,menuShowGraph);
        if(menuShowPoints)
            mpMapDrawer->DrawMapPoints();

        pangolin::FinishFrame();
        
        KeyFrame* pKF = mpTracker->mpLastKeyFrame;
        cv::Mat im;
        if(pKF>0 && !initialized){

            int i = 0;                
            for(set<MapPoint*>::iterator vit=pKF->GetMapPoints().begin(), vend= pKF->GetMapPoints().end(); vit!=vend; vit++) {
                vpMP[i] = *vit;
                i+= 1;
                if (i==n-1) {
                    initialized = true;
                    for(i=0; i<n-1; i++) {
                        vpMP[i]->lockInPicture();
                    }

                    FillVectorWithFigure();
                    break;
                }
            }
        }

        if(initialized) {
            Frame pF= Frame(mpTracker->mCurrentFrame);
            cv::Mat tcw2 = mpTracker->mCurrentFrame.mTcw;
            if(!tcw2.empty()) {
                pF.SetPose(mpTracker->mCurrentFrame.mTcw);
                for (int i = 0; i < n; i++) {
                    if (vpMP[i] > 0) {
                        pF.isInFrustumCustom(vpMP[i], 0.5);
                    }
                    
                }
            }

        //     // cout << "pKF->GetPose(): " << pKF->GetPose() <<  "\n";
        //     // bool inView = pF.isInFrustumPrint(pNewMP, 0.5);
        //     // cout << "mTrackProjX: " << pNewMP->mTrackProjX <<  "\n";
        //     // cout << "GetWorldPos: " << pNewMP->GetWorldPos() <<  "\n";
        //     // cout << "mTrackProjY: " << pNewMP->mTrackProjY <<  "\n";
        //     // cout << "mTrackProjXR: " << pNewMP->mTrackProjXR <<  "\n";
        //     // cout << "mbTrackInView: " << pNewMP->mbTrackInView <<  "\n";
        //     // cout << "mnTrackScaleLevel: " << pNewMP->mnTrackScaleLevel <<  "\n";
        //     // cout << "mTrackViewCos: " << pNewMP->mTrackViewCos <<  "\n";
        //     // cout << "mnTrackReferenceForFrame: " << pNewMP->mnTrackReferenceForFrame <<  "\n";
        //     // cout << "mnLastFrameSeen: " << pNewMP->mnLastFrameSeen <<  "\n\n\n";
             im = mpFrameDrawer->CustomDrawFrame(vpMP);
        }  else {
            im = mpFrameDrawer->DrawFrame();

         }

        cv::imshow("ORB-SLAM2: Current Frame",im);
        cv::waitKey(mT);

        if(menuReset)
        {
            menuShowGraph = true;
            menuShowKeyFrames = true;
            menuShowPoints = true;
            menuLocalizationMode = false;
            if(bLocalizationMode)
                mpSystem->DeactivateLocalizationMode();
            bLocalizationMode = false;
            bFollow = true;
            menuFollowCamera = true;
            mpSystem->Reset();
            menuReset = false;
        }

        if(Stop())
        {
            while(isStopped())
            {
                usleep(3000);
            }
        }

        if(CheckFinish())
            break;
    }

    SetFinish();
}

void Viewer::RequestFinish()
{
    unique_lock<mutex> lock(mMutexFinish);
    mbFinishRequested = true;
}

bool Viewer::CheckFinish()
{
    unique_lock<mutex> lock(mMutexFinish);
    return mbFinishRequested;
}

void Viewer::SetFinish()
{
    unique_lock<mutex> lock(mMutexFinish);
    mbFinished = true;
}

bool Viewer::isFinished()
{
    unique_lock<mutex> lock(mMutexFinish);
    return mbFinished;
}

void Viewer::RequestStop()
{
    unique_lock<mutex> lock(mMutexStop);
    if(!mbStopped)
        mbStopRequested = true;
}

bool Viewer::isStopped()
{
    unique_lock<mutex> lock(mMutexStop);
    return mbStopped;
}

bool Viewer::Stop()
{
    unique_lock<mutex> lock(mMutexStop);
    unique_lock<mutex> lock2(mMutexFinish);

    if(mbFinishRequested)
        return false;
    else if(mbStopRequested)
    {
        mbStopped = true;
        mbStopRequested = false;
        return true;
    }

    return false;

}

void Viewer::Release()
{
    unique_lock<mutex> lock(mMutexStop);
    mbStopped = false;
}

}
