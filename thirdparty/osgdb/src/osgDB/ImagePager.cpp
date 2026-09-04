/* -*-c++-*- OpenSceneGraph - Copyright (C) 1998-2006 Robert Osfield
 *
 * This library is open source and may be redistributed and/or modified under
 * the terms of the OpenSceneGraph Public License (OSGPL) version 0.0 or
 * (at your option) any later version.  The full license is in LICENSE file
 * included with this distribution, and on the openscenegraph.org website.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * OpenSceneGraph Public License for more details.
*/

#include <osgDB/ImagePager.h>
#include <osgDB/ReadFile.h>

#include <osg/Notify>
#include <osg/ImageSequence>

using namespace osgDB;


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  SortFileRequestFunctor
//
struct ImagePager::SortFileRequestFunctor
{
    bool operator() (const osg::ref_ptr<ImagePager::ImageRequest>& lhs,const osg::ref_ptr<ImagePager::ImageRequest>& rhs) const
    {
        return (lhs->_timeToMergeBy < rhs->_timeToMergeBy);
    }
};


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  RequestQueue
//
void ImagePager::RequestQueue::sort()
{
    std::sort(_requestList.begin(),_requestList.end(),SortFileRequestFunctor());
}

unsigned int ImagePager::RequestQueue::size() const
{
    return _requestList.size();
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  ReadQueue
//
ImagePager::ReadQueue::ReadQueue(ImagePager* pager, const std::string& name):
    _pager(pager),
    _name(name)
{

}

void ImagePager::ReadQueue::clear()
{

    for(RequestList::iterator citr = _requestList.begin();
        citr != _requestList.end();
        ++citr)
    {
        (*citr)->_attachmentPoint = 0;
        (*citr)->_requestQueue = 0;
    }

    _requestList.clear();

    updateBlock();
}

void ImagePager::ReadQueue::add(ImagePager::ImageRequest* imageRequest)
{
    _requestList.push_back(imageRequest);
    imageRequest->_requestQueue = this;

    OSG_INFO<<"ImagePager::ReadQueue::add("<<imageRequest->_fileName<<"), size()="<<_requestList.size()<<std::endl;

    updateBlock();
}

void ImagePager::ReadQueue::takeFirst(osg::ref_ptr<ImageRequest>& databaseRequest)
{
    if (!_requestList.empty())
    {
        sort();

        OSG_INFO<<"ImagePager::ReadQueue::takeFirst(..), size()="<<_requestList.size()<<std::endl;

        databaseRequest = _requestList.front();
        databaseRequest->_requestQueue = 0;
        _requestList.erase(_requestList.begin());

        updateBlock();
    }
}

//////////////////////////////////////////////////////////////////////////////////////
//
// ImageThread
//
ImagePager::ImageThread::ImageThread(ImagePager* pager, Mode mode, const std::string& name):
    _done(false),
    _mode(mode),
    _pager(pager),
    _name(name)
{
}

ImagePager::ImageThread::ImageThread(const ImageThread& dt, ImagePager* pager):
    _done(false),
    _mode(dt._mode),
    _pager(pager),
    _name(dt._name)
{
}

ImagePager::ImageThread::~ImageThread()
{
}

int ImagePager::ImageThread::cancel()
{
    return 0;
}

void ImagePager::signalBeginFrame(const osg::FrameStamp* framestamp)
{
    if (framestamp)
    {
        //OSG_INFO << "signalBeginFrame "<<framestamp->getFrameNumber()<<">>>>>>>>>>>>>>>>"<<std::endl;
        _frameNumber =framestamp->getFrameNumber();

    } //else OSG_INFO << "signalBeginFrame >>>>>>>>>>>>>>>>"<<std::endl;
}

void ImagePager::signalEndFrame()
{
}


void ImagePager::ImageThread::run()
{

}

//////////////////////////////////////////////////////////////////////////////////////
//
// ImagePager
//
ImagePager::ImagePager():
    _done(false)
{
    _startThreadCalled = false;
    _databasePagerThreadPaused = false;

    _readQueue = new ReadQueue(this,"Image Queue");
    _completedQueue = new RequestQueue;
    // 1 second
    _preLoadTime = 1.0;
}

ImagePager::~ImagePager()
{
    cancel();
}

int ImagePager::cancel()
{
    _done = true;
    _startThreadCalled = false;

    //std::cout<<"DatabasePager::~DatabasePager() stopped running"<<std::endl;
    return 0;
}

osg::ref_ptr<osg::Image> ImagePager::readRefImageFile(const std::string& fileName, const osg::Referenced* options)
{
    osgDB::Options* readOptions = dynamic_cast<osgDB::Options*>(const_cast<osg::Referenced*>(options));
    return osgDB::readRefImageFile(fileName, readOptions);
}

void ImagePager::requestImageFile(const std::string& fileName, osg::Object* attachmentPoint, int attachmentIndex, double timeToMergeBy, const osg::FrameStamp* /*framestamp*/, osg::ref_ptr<osg::Referenced>& imageRequest, const osg::Referenced* options)
{
    
}

bool ImagePager::requiresUpdateSceneGraph() const
{
    //OSG_NOTICE<<"ImagePager::requiresUpdateSceneGraph()"<<std::endl;
    return !(_completedQueue->_requestList.empty());
}

void ImagePager::updateSceneGraph(const osg::FrameStamp&)
{

}

