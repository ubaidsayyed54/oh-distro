#include "signaldata.h"

#include <qvector.h>
#include <qmutex.h>
#include <qreadwritelock.h>


#include "fpscounter.h"


class SignalData::PrivateData
{
public:
  PrivateData()
  {
    messageError = false;
  }

  bool messageError;

  QReadWriteLock lock;

  QRectF boundingRect;

  QMutex mutex; // protecting pending values
  QVector<double> xvalues;
  QVector<double> yvalues;
  QVector<double> pendingxvalues;
  QVector<double> pendingyvalues;

  FPSCounter fpsCounter;
};

SignalData::SignalData()
{
  d_data = new PrivateData();
}

SignalData::~SignalData()
{
  delete d_data;
}

int SignalData::size() const
{
  return d_data->xvalues.size();
}

QPointF SignalData::value(int index) const
{
  return QPointF(d_data->xvalues[index], d_data->yvalues[index]);
}

QRectF SignalData::boundingRect() const
{
  return d_data->boundingRect;
}

void SignalData::lock()
{
  d_data->mutex.lock();
}

void SignalData::unlock()
{
  d_data->mutex.unlock();
}

void SignalData::appendSample(double x, double y)
{
  d_data->mutex.lock();
  d_data->pendingxvalues.append(x);
  d_data->pendingyvalues.append(y);
  d_data->fpsCounter.update();
  d_data->mutex.unlock();
}

void SignalData::clear()
{
  d_data->mutex.lock();
  d_data->xvalues.clear();
  d_data->yvalues.clear();
  d_data->pendingxvalues.clear();
  d_data->pendingyvalues.clear();
  d_data->boundingRect = QRectF();
  d_data->mutex.unlock();
}

void SignalData::flagMessageError()
{
  d_data->messageError = true;
}

bool SignalData::hasMessageError() const
{
  return d_data->messageError;
}

double SignalData::messageFrequency() const
{
  d_data->mutex.lock();
  double freq = d_data->fpsCounter.averageFPS();
  d_data->mutex.unlock();
  return freq;
}

void SignalData::updateValues()
{
  QVector<double>& xvalues = d_data->xvalues;
  QVector<double>& yvalues = d_data->yvalues;

  d_data->mutex.lock();
  xvalues += d_data->pendingxvalues;
  yvalues += d_data->pendingyvalues;
  d_data->pendingxvalues.clear();
  d_data->pendingyvalues.clear();
  d_data->mutex.unlock();

  if (!xvalues.size())
  {
    return;
  }

  // All values that are older than 60 seconds will expire
  float expireTime = xvalues.back() - 60;

  int idx = 0;
  while (idx < xvalues.size() && xvalues[idx] < expireTime)
  {
    ++idx;
  }

  // if itr == begin(), then no values will be erased
  xvalues.erase(xvalues.begin(), xvalues.begin()+idx);
  yvalues.erase(yvalues.begin(), yvalues.begin()+idx);


  // recompute bounding rect
  if (xvalues.size() > 1)
  {
    double minY = yvalues.front();
    double maxY = minY;

    double minX = xvalues.front();
    double maxX = minX;

    for (int i = 0; i < yvalues.size(); ++i)
    {
      double sampleY = yvalues[i];
      double sampleX = xvalues[i];

      if (sampleY < minY)
        minY = sampleY;

      if (sampleY > maxY)
        maxY = sampleY;

      if (sampleX < minX)
        minX = sampleX;

      if (sampleX > maxX)
        maxX = sampleX;
    }

    d_data->boundingRect.setLeft(minX);
    d_data->boundingRect.setRight(maxX);

    d_data->boundingRect.setTop(maxY);
    d_data->boundingRect.setBottom(minY);
  }
  else
  {
    d_data->boundingRect = QRectF();
  }

}
