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


#include <osg/OperationThread>
#include <osg/GraphicsContext>
#include <osg/Notify>

using namespace osg;

struct BlockOperation : public Operation
{
    BlockOperation():
        osg::Referenced(true),
        Operation("Block",false)
    {
    }

    virtual void release()
    {
    }

    virtual void operator () (Object*)
    {
    }
};

/////////////////////////////////////////////////////////////////////////////
//
//  OperationsQueue
//

OperationQueue::OperationQueue():
    osg::Referenced(true)
{
    _currentOperationIterator = _operations.begin();
    _operationsBlock = new RefBlock;
}

OperationQueue::~OperationQueue()
{
}

bool OperationQueue::empty()
{

  return _operations.empty();
}

unsigned int OperationQueue::getNumOperationsInQueue()
{
  return static_cast<unsigned int>(_operations.size());
}

ref_ptr<Operation> OperationQueue::getNextOperation(bool blockIfEmpty)
{
    if (blockIfEmpty && _operations.empty())
    {
        
    }

    if (_operations.empty()) return osg::ref_ptr<Operation>();

    if (_currentOperationIterator == _operations.end())
    {
        // iterator at end of operations so reset to beginning.
        _currentOperationIterator = _operations.begin();
    }

    ref_ptr<Operation> currentOperation = *_currentOperationIterator;

    if (!currentOperation->getKeep())
    {
        // OSG_INFO<<"removing "<<currentOperation->getName()<<std::endl;

        // remove it from the operations queue
        _currentOperationIterator = _operations.erase(_currentOperationIterator);

        // OSG_INFO<<"size "<<_operations.size()<<std::endl;

        if (_operations.empty())
        {
        }
    }
    else
    {
        // OSG_INFO<<"increment "<<_currentOperation->getName()<<std::endl;

        // move on to the next operation in the list.
        ++_currentOperationIterator;
    }

    return currentOperation;
}

void OperationQueue::add(Operation* operation)
{
    OSG_INFO<<"Doing add"<<std::endl;

    // add the operation to the end of the list
    _operations.push_back(operation);
}

void OperationQueue::remove(Operation* operation)
{
    OSG_INFO<<"Doing remove operation"<<std::endl;

    for(Operations::iterator itr = _operations.begin();
        itr!=_operations.end();)
    {
        if ((*itr)==operation)
        {
            bool needToResetCurrentIterator = (_currentOperationIterator == itr);

            itr = _operations.erase(itr);

            if (needToResetCurrentIterator)
            {
                _currentOperationIterator = (itr==_operations.end()) ? _operations.begin() : itr;
            }

        }
        else ++itr;
    }
}

void OperationQueue::remove(const std::string& name)
{
    OSG_INFO<<"Doing remove named operation"<<std::endl;

    // find the remove all operations with specified name
    for(Operations::iterator itr = _operations.begin();
        itr!=_operations.end();)
    {
        if ((*itr)->getName()==name)
        {
            bool needToResetCurrentIterator = (_currentOperationIterator == itr);

            itr = _operations.erase(itr);

            if (needToResetCurrentIterator)
            {
                _currentOperationIterator = (itr==_operations.end()) ? _operations.begin() : itr;
            }
        }
        else ++itr;
    }

    if (_operations.empty())
    {
    }
}

void OperationQueue::removeAllOperations()
{
    OSG_INFO<<"Doing remove all operations"<<std::endl;

    _operations.clear();

    // reset current operator.
    _currentOperationIterator = _operations.begin();

    if (_operations.empty())
    {
    }
}

void OperationQueue::runOperations(Object* callingObject)
{
    // reset current operation iterator to beginning if at end.
    if (_currentOperationIterator==_operations.end()) _currentOperationIterator = _operations.begin();

    for(;
        _currentOperationIterator != _operations.end();
        )
    {
        ref_ptr<Operation> operation = *_currentOperationIterator;

        if (!operation->getKeep())
        {
            _currentOperationIterator = _operations.erase(_currentOperationIterator);
        }
        else
        {
            ++_currentOperationIterator;
        }

        // OSG_INFO<<"Doing op "<<_currentOperation->getName()<<" "<<this<<std::endl;

        // call the graphics operation.
        (*operation)(callingObject);
    }

    if (_operations.empty())
    {
    }
}

void OperationQueue::releaseOperationsBlock()
{
}

 void OperationQueue::releaseAllOperations()
{
    for(Operations::iterator itr = _operations.begin();
        itr!=_operations.end();
        ++itr)
    {
        (*itr)->release();
    }
}


void OperationQueue::addOperationThread(OperationThread* thread)
{
    _operationThreads.insert(thread);
}

void OperationQueue::removeOperationThread(OperationThread* thread)
{
    _operationThreads.erase(thread);
}

/////////////////////////////////////////////////////////////////////////////
//
//  OperationThread
//

OperationThread::OperationThread():
    osg::Referenced(true),
    _parent(0),
    _done(0)
{
    setOperationQueue(new OperationQueue);
}

OperationThread::~OperationThread()
{
    //OSG_NOTICE<<"Destructing graphics thread "<<this<<std::endl;

    cancel();

    //OSG_NOTICE<<"Done Destructing graphics thread "<<this<<std::endl;
}

void OperationThread::setOperationQueue(OperationQueue* opq)
{
    if (_operationQueue == opq) return;

    if (_operationQueue.valid()) _operationQueue->removeOperationThread(this);

    _operationQueue = opq;

    if (_operationQueue.valid()) _operationQueue->addOperationThread(this);
}

void OperationThread::setDone(bool done)
{
    unsigned d = done?1:0;
    if (_done==d) return;

    _done = d;

    if (done)
    {
        OSG_INFO<<"set done "<<this<<std::endl;

        {
            if (_currentOperation.valid())
            {
                OSG_INFO<<"releasing "<<_currentOperation.get()<<std::endl;
                _currentOperation->release();
            }
        }

        if (_operationQueue.valid()) _operationQueue->releaseOperationsBlock();
    }
}

int OperationThread::cancel()
{

    return 0;
}

void OperationThread::add(Operation* operation)
{
    if (!_operationQueue) _operationQueue = new OperationQueue;
    _operationQueue->add(operation);
}

void OperationThread::remove(Operation* operation)
{
    if (_operationQueue.valid()) _operationQueue->remove(operation);
}

void OperationThread::remove(const std::string& name)
{
    if (_operationQueue.valid()) _operationQueue->remove(name);
}

void OperationThread::removeAllOperations()
{
    if (_operationQueue.valid()) _operationQueue->removeAllOperations();
}

void OperationThread::run()
{

}
