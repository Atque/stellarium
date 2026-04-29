/*
 * Daylight Map plug-in for Stellarium
 *
 * Copyright (C) 2026 Atque
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "SunriseSunsetMapWidget.hpp"

#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelLocation.hpp"
#include "StelLocationMgr.hpp"
#include "StelModuleMgr.hpp"
#include "StelSkyDrawer.hpp"
#include "StelTranslator.hpp"
#include "StelUtils.hpp"
#include "VecMath.hpp"
#include "Planet.hpp"
#include "RefractionExtinction.hpp"
#include "SolarSystem.hpp"
#include "planetsephems/sidereal_time.h"

#include <algorithm>
#include <QDate>
#include <QDateTime>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QTime>
#include <QWheelEvent>
#include <QtMath>

namespace
{
constexpr double mapCenterLongitudeDeg = 0.0;
constexpr double earthEquatorialRadiusAu = 6378.137 / 149597870.7;
constexpr double moonStandardRefractionDeg = 0.5667;
constexpr double minutesPerDay = 1440.0;
constexpr double eventWrapThresholdMinutes = minutesPerDay / 2.0;
double normalizeLongitudeDeg(double longitudeDeg)
{
	while (longitudeDeg < -180.0)
		longitudeDeg += 360.0;
	while (longitudeDeg > 180.0)
		longitudeDeg -= 360.0;
	return longitudeDeg;
}

QPointF interpolatedPoint(const QPointF& a, const QPointF& b, double valueA, double valueB, double level)
{
	const double denom = valueB - valueA;
	const double t = qFuzzyIsNull(denom) ? 0.5 : (level - valueA) / denom;
	return a + (b - a) * qBound(0.0, t, 1.0);
}
}

SunriseSunsetMapWidget::SunriseSunsetMapWidget(QWidget* parent)
	: QWidget(parent)
	, earthSvg(QStringLiteral(":/DaylightMap/earth.svg"))
	, core(StelApp::getInstance().getCore())
	, bodyMode(Sun)
	, eventMode(Sunrise)
	, showGrid(false)
	, centerLongitudeDeg(mapCenterLongitudeDeg)
	, centerLatitudeDeg(0.)
	, longitudeSpanDeg(360.)
	, dragging(false)
	, sceneCacheDirty(true)
	, currentJD(0.)
	, utcOffsetHours(0.)
	, year(2000)
	, month(1)
	, day(1)
	, dayOfYear(1)
{
	setMinimumSize(720, 360);
	setMouseTracking(true);
	updateDateCache();
}

void SunriseSunsetMapWidget::updateFromCore()
{
	updateDateCache();
	invalidateSceneCache();
	update();
}

void SunriseSunsetMapWidget::setBodyMode(int mode)
{
	bodyMode = (mode == Moon) ? Moon : Sun;
	invalidateSceneCache();
	update();
}

void SunriseSunsetMapWidget::setEventMode(int mode)
{
	eventMode = (mode == Sunset) ? Sunset : Sunrise;
	invalidateSceneCache();
	update();
}

void SunriseSunsetMapWidget::setFlagShowGrid(bool show)
{
	if (showGrid == show)
		return;

	showGrid = show;
	invalidateSceneCache();
	update();
}

void SunriseSunsetMapWidget::resetView()
{
	centerLongitudeDeg = mapCenterLongitudeDeg;
	centerLatitudeDeg = 0.;
	longitudeSpanDeg = 360.;
	invalidateSceneCache();
	update();
}

void SunriseSunsetMapWidget::zoomToCurrentLocation()
{
	if (!core || core->getCurrentLocation().planetName != "Earth")
		return;

	centerLongitudeDeg = static_cast<double>(core->getCurrentLocation().getLongitude());
	centerLatitudeDeg = static_cast<double>(core->getCurrentLocation().getLatitude());
	longitudeSpanDeg = 70.;
	constrainView();
	invalidateSceneCache();
	update();
}

void SunriseSunsetMapWidget::addDays(int days)
{
	if (!core)
		return;

	core->setJD(core->getJD() + static_cast<double>(days));
	updateFromCore();
}

void SunriseSunsetMapWidget::paintEvent(QPaintEvent* event)
{
	Q_UNUSED(event)

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	QRectF mapRect = rect().adjusted(10, 10, -10, -34);
	if (sceneCacheDirty || sceneCacheSize != size())
		rebuildSceneCache(mapRect);

	painter.drawPixmap(0, 0, sceneCache);
	painter.setClipRect(mapRect);
	drawCurrentLocation(painter, mapRect);
	painter.setClipping(false);

	painter.setPen(QPen(Qt::black, 2));
	painter.setBrush(Qt::NoBrush);
	painter.drawRect(mapRect);

	painter.setPen(Qt::black);
	painter.drawText(QRectF(mapRect.left(), mapRect.bottom() + 5,
	                        mapRect.width(), height() - mapRect.bottom() - 6),
	                 Qt::AlignCenter, titleText());
}

void SunriseSunsetMapWidget::mousePressEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		dragging = true;
		lastMousePos = event->pos();
		event->accept();
		return;
	}
	QWidget::mousePressEvent(event);
}

void SunriseSunsetMapWidget::mouseMoveEvent(QMouseEvent* event)
{
	if (dragging)
	{
		const QPoint delta = event->pos() - lastMousePos;
		lastMousePos = event->pos();

		const QRectF mapRect = rect().adjusted(10, 10, -10, -34);
		const double latitudeSpanDeg = longitudeSpanDeg * mapRect.height() / mapRect.width();
		centerLongitudeDeg = normalizeLongitudeDeg(centerLongitudeDeg - delta.x() * longitudeSpanDeg / mapRect.width());
		centerLatitudeDeg += delta.y() * latitudeSpanDeg / mapRect.height();
		constrainView();
		invalidateSceneCache();
		update();
		event->accept();
		return;
	}
	QWidget::mouseMoveEvent(event);
}

void SunriseSunsetMapWidget::mouseReleaseEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		dragging = false;
		event->accept();
		return;
	}
	QWidget::mouseReleaseEvent(event);
}

void SunriseSunsetMapWidget::wheelEvent(QWheelEvent* event)
{
	const QRectF mapRect = rect().adjusted(10, 10, -10, -34);
	double cursorLon = centerLongitudeDeg;
	double cursorLat = centerLatitudeDeg;
	screenToLonLat(event->position(), mapRect, cursorLon, cursorLat);

	const double factor = event->angleDelta().y() > 0 ? 0.78 : 1.28;
	longitudeSpanDeg = qBound(20.0, longitudeSpanDeg * factor, 360.0);

	const double latitudeSpanDeg = longitudeSpanDeg * mapRect.height() / mapRect.width();
	const double xFrac = (event->position().x() - mapRect.left()) / mapRect.width() - 0.5;
	const double yFrac = 0.5 - (event->position().y() - mapRect.top()) / mapRect.height();
	centerLongitudeDeg = normalizeLongitudeDeg(cursorLon - xFrac * longitudeSpanDeg);
	centerLatitudeDeg = cursorLat - yFrac * latitudeSpanDeg;
	constrainView();
	invalidateSceneCache();
	update();
	event->accept();
}

double SunriseSunsetMapWidget::localEventMinutes(double latitudeDeg, double longitudeDeg,
                                                 const QVector<BodySample>& bodySamples) const
{
	return localSampledEventMinutes(latitudeDeg, longitudeDeg, bodySamples);
}

double SunriseSunsetMapWidget::localSampledEventMinutes(double latitudeDeg, double longitudeDeg,
                                                        const QVector<BodySample>& bodySamples) const
{
	if (bodySamples.size() < 2)
		return qQNaN();

	const double latitudeRad = latitudeDeg * M_PI / 180.0;
	const double longitudeRad = longitudeDeg * M_PI / 180.0;
	double previousAltitude = sampledBodyAltitudeMinusHorizon(latitudeRad, longitudeRad, bodySamples.first());

	for (int i = 1; i < bodySamples.size(); ++i)
	{
		const double altitude = sampledBodyAltitudeMinusHorizon(latitudeRad, longitudeRad, bodySamples.at(i));
		const bool crossing = eventMode == Sunrise
		                      ? (previousAltitude < 0.0 && altitude >= 0.0)
		                      : (previousAltitude >= 0.0 && altitude < 0.0);
		if (crossing)
		{
			const double denom = previousAltitude - altitude;
			const double t = qFuzzyIsNull(denom) ? 0.5 : previousAltitude / denom;
			return bodySamples.at(i - 1).minutes + qBound(0.0, t, 1.0) *
			       (bodySamples.at(i).minutes - bodySamples.at(i - 1).minutes);
		}
		previousAltitude = altitude;
	}

	return qQNaN();
}

double SunriseSunsetMapWidget::sampledBodyAltitudeMinusHorizon(double latitudeRad, double longitudeRad,
                                                               const BodySample& sample) const
{
	double hourAngleRad = sample.siderealRad + longitudeRad - sample.rightAscensionRad;
	while (hourAngleRad < -M_PI)
		hourAngleRad += 2.0 * M_PI;
	while (hourAngleRad > M_PI)
		hourAngleRad -= 2.0 * M_PI;

	const double altitudeRad = qAsin(qSin(latitudeRad) * qSin(sample.declinationRad) +
	                                qCos(latitudeRad) * qCos(sample.declinationRad) * qCos(hourAngleRad));
	return altitudeRad - sample.horizonAltitudeRad;
}

QVector<SunriseSunsetMapWidget::BodySample> SunriseSunsetMapWidget::buildBodySamples() const
{
	QVector<BodySample> samples;
	if (!core)
		return samples;

	SolarSystem* solarSystem = GETSTELMODULE(SolarSystem);
	if (!solarSystem || solarSystem->getEarth().isNull())
		return samples;

	const QDate date(year, month, day);
	if (!date.isValid())
		return samples;

	const double localMidnightJD = StelUtils::qDateTimeToJd(QDateTime(date, QTime(0, 0, 0), Qt::UTC));
	const double utcStartJD = localMidnightJD - utcOffsetHours / 24.0;
	const PlanetP earth = solarSystem->getEarth();
	const PlanetP body = bodyMode == Moon ? solarSystem->getMoon() : solarSystem->getSun();
	if (body.isNull())
		return samples;

	double refractionOffsetRad = 0.0;
	if (bodyMode == Sun && core->getSkyDrawer()->getFlagHasAtmosphere())
	{
		Refraction refraction = core->getSkyDrawer()->getRefraction();
		Vec3d zeroAlt(1.0, 0.0, 0.0);
		refraction.backward(zeroAlt);
		refractionOffsetRad = qAsin(qBound(-1.0, zeroAlt[2], 1.0));
	}

	const int sampleCount = bodyMode == Sun ? 97 : 49;
	samples.reserve(sampleCount);
	for (int i = 0; i < sampleCount; ++i)
	{
		const double minutes = i * 1440.0 / static_cast<double>(sampleCount - 1);
		const double jd = utcStartJD + minutes / 1440.0;
		const double jde = jd + core->computeDeltaT(jd) / 86400.0;
		const Vec3d earthPos = earth->getHeliocentricEclipticPos(jde);
		const Vec3d bodyVector = body->getHeliocentricEclipticPos(jde) - earthPos;
		const Vec3d equatorial = StelCore::matVsop87ToJ2000.multiplyWithoutTranslation(bodyVector);

		BodySample sample;
		sample.minutes = minutes;
		StelUtils::rectToSphe(&sample.rightAscensionRad, &sample.declinationRad, equatorial);
		sample.rightAscensionRad = StelUtils::fmodpos(sample.rightAscensionRad, 2.0 * M_PI);
		sample.siderealRad = get_apparent_sidereal_time(jd, jde) * M_PI / 180.0;

		if (bodyMode == Moon)
		{
			const double horizontalParallaxRad = qAsin(qBound(-1.0, earthEquatorialRadiusAu / bodyVector.norm(), 1.0));
			sample.horizonAltitudeRad = 0.7275 * horizontalParallaxRad - moonStandardRefractionDeg * M_PI / 180.0;
		}
		else
		{
			sample.horizonAltitudeRad = -qAtan(body->getEquatorialRadius() / bodyVector.norm()) + refractionOffsetRad;
		}
		samples << sample;
	}

	return samples;
}

double SunriseSunsetMapWidget::axialTiltDeg() const
{
	SolarSystem* solarSystem = GETSTELMODULE(SolarSystem);
	if (!core || !solarSystem || solarSystem->getEarth().isNull())
		return 23.4392911;

	return solarSystem->getEarth()->getRotObliquity(core->getJDE()) * M_180_PI;
}

QString SunriseSunsetMapWidget::minutesToString(double minutes) const
{
	int wholeMinutes = static_cast<int>(qFloor(minutes + 0.5));
	while (wholeMinutes < 0)
		wholeMinutes += 24 * 60;
	while (wholeMinutes >= 24 * 60)
		wholeMinutes -= 24 * 60;

	return QString::asprintf("%02d.%02d", wholeMinutes / 60, wholeMinutes % 60);
}

QString SunriseSunsetMapWidget::titleText() const
{
	QString eventName;
	if (bodyMode == Sun)
		eventName = eventMode == Sunrise ? q_("Sunrise") : q_("Sunset");
	else
		eventName = eventMode == Sunrise ? q_("Moonrise") : q_("Moonset");

	return eventName + QStringLiteral("  ") +
	       QString::asprintf("%04d-%02d-%02d  UTC%+g", year, month, day, utcOffsetHours);
}

QPointF SunriseSunsetMapWidget::lonLatToPoint(double longitudeDeg, double latitudeDeg, const QRectF& mapRect) const
{
	const double latitudeSpanDeg = longitudeSpanDeg * mapRect.height() / mapRect.width();
	const double x = mapRect.center().x() + normalizeLongitudeDeg(longitudeDeg - centerLongitudeDeg) / longitudeSpanDeg * mapRect.width();
	const double y = mapRect.center().y() - (latitudeDeg - centerLatitudeDeg) / latitudeSpanDeg * mapRect.height();
	return QPointF(x, y);
}

void SunriseSunsetMapWidget::screenToLonLat(const QPointF& point, const QRectF& mapRect,
                                            double& longitudeDeg, double& latitudeDeg) const
{
	const double latitudeSpanDeg = longitudeSpanDeg * mapRect.height() / mapRect.width();
	longitudeDeg = normalizeLongitudeDeg(centerLongitudeDeg + (point.x() - mapRect.center().x()) * longitudeSpanDeg / mapRect.width());
	latitudeDeg = centerLatitudeDeg - (point.y() - mapRect.center().y()) * latitudeSpanDeg / mapRect.height();
	latitudeDeg = qBound(-89.0, latitudeDeg, 89.0);
}

void SunriseSunsetMapWidget::drawBaseMap(QPainter& painter, const QRectF& mapRect)
{
	const double latitudeSpanDeg = longitudeSpanDeg * mapRect.height() / mapRect.width();
	const QRectF destBase(0., mapRect.center().y() - (90.0 - centerLatitudeDeg) / latitudeSpanDeg * mapRect.height(),
	                      mapRect.width() * 360.0 / longitudeSpanDeg,
	                      mapRect.height() * 180.0 / latitudeSpanDeg);

	for (int wrap = -2; wrap <= 2; ++wrap)
	{
		const double lonLeft = mapCenterLongitudeDeg - 180.0 + wrap * 360.0;
		const double xLeft = mapRect.center().x() + (lonLeft - centerLongitudeDeg) / longitudeSpanDeg * mapRect.width();
		QRectF dest = destBase;
		dest.moveLeft(xLeft);
		if (earthSvg.isValid())
			earthSvg.render(&painter, dest);
	}
}

void SunriseSunsetMapWidget::drawIsolines(QPainter& painter, const QRectF& mapRect)
{
	const int columns = bodyMode == Moon ? qBound(60, qRound(mapRect.width() / 8.0), 140)
	                                     : qBound(80, qRound(mapRect.width() / 5.0), 220);
	const int rows = bodyMode == Moon ? qBound(40, qRound(mapRect.height() / 8.0), 90)
	                                  : qBound(50, qRound(mapRect.height() / 5.0), 140);
	const QVector<BodySample> bodySamples = buildBodySamples();
	const double latitudeSpanDeg = longitudeSpanDeg * mapRect.height() / mapRect.width();
	QVector<QVector<Sample> > grid(rows + 1, QVector<Sample>(columns + 1));

	double minValue = 1.0e9;
	double maxValue = -1.0e9;
	for (int y = 0; y <= rows; ++y)
	{
		const double lat = centerLatitudeDeg + latitudeSpanDeg / 2.0 - static_cast<double>(y) * latitudeSpanDeg / rows;
		for (int x = 0; x <= columns; ++x)
		{
			const double lon = normalizeLongitudeDeg(centerLongitudeDeg - longitudeSpanDeg / 2.0 +
			                                        static_cast<double>(x) * longitudeSpanDeg / columns);
			const double value = localEventMinutes(lat, lon, bodySamples);
			if (!qIsNaN(value))
			{
				grid[y][x].value = value;
				grid[y][x].valid = true;
				minValue = qMin(minValue, value);
				maxValue = qMax(maxValue, value);
			}
		}
	}

	if (minValue > maxValue)
	{
		painter.setPen(QColor(40, 40, 40));
		const QString text = bodyMode == Moon ? q_("No moonrise/moonset event in this view")
		                                      : q_("No sunrise/sunset event in this view");
		painter.drawText(mapRect, Qt::AlignCenter, text);
		return;
	}

	const double minorStep = longitudeSpanDeg > 160.0 ? 30.0 : (longitudeSpanDeg > 70.0 ? 15.0 : 5.0);
	const double majorStep = qMax(15.0, minorStep * 3.0);
	const double start = qFloor(minValue / minorStep) * minorStep;
	const double stop = qCeil(maxValue / minorStep) * minorStep;

	for (double level = start; level <= stop; level += minorStep)
	{
		if (level > 0.0 && qAbs(std::fmod(level, minutesPerDay)) < 1.0e-6)
			continue;

		const bool major = qFuzzyIsNull(std::fmod(qAbs(level), majorStep));
		const QPen pen(major ? QColor(0, 0, 0, 220) : QColor(0, 0, 0, 115),
		               major ? 2.2 : 0.8);
		drawContour(painter, grid, mapRect, level, pen, major);
	}
}

void SunriseSunsetMapWidget::drawContour(QPainter& painter, const QVector<QVector<Sample> >& grid,
                                         const QRectF& mapRect, double level, const QPen& pen, bool labelEdges)
{
	const int rows = grid.size() - 1;
	const int columns = grid[0].size() - 1;
	painter.setPen(pen);

	auto pointAt = [&](int ix, int iy) {
		return QPointF(mapRect.left() + static_cast<double>(ix) * mapRect.width() / columns,
		               mapRect.top() + static_cast<double>(iy) * mapRect.height() / rows);
	};

	int labelsDrawn = 0;
	for (int y = 0; y < rows; ++y)
	{
		for (int x = 0; x < columns; ++x)
		{
			const Sample s0 = grid[y][x];
			const Sample s1 = grid[y][x + 1];
			const Sample s2 = grid[y + 1][x + 1];
			const Sample s3 = grid[y + 1][x];
			const QPointF p0 = pointAt(x, y);
			const QPointF p1 = pointAt(x + 1, y);
			const QPointF p2 = pointAt(x + 1, y + 1);
			const QPointF p3 = pointAt(x, y + 1);

			QVector<QPointF> crossings;
			const bool midnightLevel = qAbs(std::fmod(level, minutesPerDay)) < 1.0e-6;
			auto appendCrossing = [&](const Sample& a, const Sample& b, const QPointF& pa, const QPointF& pb) {
				if (!a.valid || !b.valid)
					return;

				if (qAbs(a.value - b.value) > eventWrapThresholdMinutes)
				{
					if (!midnightLevel)
						return;

					const double unwrappedB = a.value > b.value ? b.value + minutesPerDay : b.value - minutesPerDay;
					const double target = a.value > b.value ? minutesPerDay : 0.0;
					const double denom = unwrappedB - a.value;
					const double t = qFuzzyIsNull(denom) ? 0.5 : (target - a.value) / denom;
					crossings << pa + (pb - pa) * qBound(0.0, t, 1.0);
					return;
				}

				if ((a.value <= level && b.value > level) || (a.value > level && b.value <= level))
					crossings << interpolatedPoint(pa, pb, a.value, b.value, level);
			};

			appendCrossing(s0, s1, p0, p1);
			appendCrossing(s1, s2, p1, p2);
			appendCrossing(s2, s3, p2, p3);
			appendCrossing(s3, s0, p3, p0);

			if (crossings.size() == 2)
				painter.drawLine(crossings.at(0), crossings.at(1));
			else if (crossings.size() == 4)
			{
				painter.drawLine(crossings.at(0), crossings.at(1));
				painter.drawLine(crossings.at(2), crossings.at(3));
			}

			if (labelEdges && labelsDrawn < 8)
			{
				for (const QPointF& p : std::as_const(crossings))
				{
					const bool edge = qAbs(p.x() - mapRect.left()) < 1.5 || qAbs(p.x() - mapRect.right()) < 1.5 ||
					                  qAbs(p.y() - mapRect.top()) < 1.5 || qAbs(p.y() - mapRect.bottom()) < 1.5;
					if (edge)
					{
						drawEdgeLabel(painter, p, mapRect, minutesToString(level));
						++labelsDrawn;
						break;
					}
				}
			}
		}
	}
}

void SunriseSunsetMapWidget::drawEdgeLabel(QPainter& painter, const QPointF& point, const QRectF& mapRect, const QString& text)
{
	painter.save();
	painter.setPen(Qt::NoPen);
	painter.setBrush(QColor(255, 255, 255, 210));

	QRectF labelRect(point.x() - 20., point.y() - 9., 40., 18.);
	if (qAbs(point.x() - mapRect.left()) < 1.5)
		labelRect.moveLeft(mapRect.left() + 3.);
	else if (qAbs(point.x() - mapRect.right()) < 1.5)
		labelRect.moveRight(mapRect.right() - 3.);
	if (qAbs(point.y() - mapRect.top()) < 1.5)
		labelRect.moveTop(mapRect.top() + 3.);
	else if (qAbs(point.y() - mapRect.bottom()) < 1.5)
		labelRect.moveBottom(mapRect.bottom() - 3.);

	painter.drawRect(labelRect);
	painter.setPen(Qt::black);
	painter.drawText(labelRect, Qt::AlignCenter, text);
	painter.restore();
}

void SunriseSunsetMapWidget::rebuildSceneCache(const QRectF& mapRect)
{
	const double devicePixelRatio = devicePixelRatioF();
	sceneCache = QPixmap(QSize(qCeil(width() * devicePixelRatio),
	                           qCeil(height() * devicePixelRatio)));
	sceneCache.setDevicePixelRatio(devicePixelRatio);
	sceneCache.fill(Qt::transparent);

	QPainter cachePainter(&sceneCache);
	cachePainter.setRenderHint(QPainter::Antialiasing);
	cachePainter.fillRect(rect(), QColor(245, 245, 242));
	cachePainter.setClipRect(mapRect);
	drawBaseMap(cachePainter, mapRect);
	if (showGrid)
		drawGeographicGrid(cachePainter, mapRect);
	drawLocationLabels(cachePainter, mapRect);
	drawIsolines(cachePainter, mapRect);
	cachePainter.setClipping(false);

	sceneCacheSize = size();
	sceneCacheDirty = false;
}

void SunriseSunsetMapWidget::invalidateSceneCache()
{
	sceneCacheDirty = true;
}

void SunriseSunsetMapWidget::drawCurrentLocation(QPainter& painter, const QRectF& mapRect) const
{
	if (!core || core->getCurrentLocation().planetName != "Earth")
		return;

	const QPointF p = lonLatToPoint(static_cast<double>(core->getCurrentLocation().getLongitude()),
	                                static_cast<double>(core->getCurrentLocation().getLatitude()),
	                                mapRect);
	if (!mapRect.adjusted(-10, -10, 10, 10).contains(p))
		return;

	painter.save();
	painter.setPen(QPen(Qt::white, 4.0));
	painter.drawEllipse(p, 5.5, 5.5);
	painter.drawLine(QPointF(p.x() - 8., p.y()), QPointF(p.x() + 8., p.y()));
	painter.drawLine(QPointF(p.x(), p.y() - 8.), QPointF(p.x(), p.y() + 8.));
	painter.setPen(QPen(Qt::black, 1.5));
	painter.drawEllipse(p, 5.5, 5.5);
	painter.drawLine(QPointF(p.x() - 8., p.y()), QPointF(p.x() + 8., p.y()));
	painter.drawLine(QPointF(p.x(), p.y() - 8.), QPointF(p.x(), p.y() + 8.));
	painter.restore();
}

void SunriseSunsetMapWidget::drawLocationLabels(QPainter& painter, const QRectF& mapRect) const
{
	if (longitudeSpanDeg > 95.0)
		return;

	const int populationThreshold = longitudeSpanDeg > 55.0 ? 750 : 150;
	LocationList locations = StelApp::getInstance().getLocationMgr().getAll();
	locations.erase(std::remove_if(locations.begin(), locations.end(), [populationThreshold](const StelLocation& loc) {
		return loc.planetName != "Earth" || loc.population < populationThreshold ||
		       (loc.role != 'C' && loc.role != 'B' && loc.role != 'R' && loc.population < 1000);
	}), locations.end());
	std::sort(locations.begin(), locations.end(), [](const StelLocation& a, const StelLocation& b) {
		return a.population > b.population;
	});

	painter.save();
	painter.setFont(QFont(painter.font().family(), longitudeSpanDeg < 45.0 ? 8 : 7));
	QVector<QRectF> usedRects;
	int drawn = 0;
	const int maxLabels = longitudeSpanDeg < 45.0 ? 35 : 20;
	for (const StelLocation& loc : std::as_const(locations))
	{
		if (drawn >= maxLabels)
			break;

		const QPointF p = lonLatToPoint(static_cast<double>(loc.getLongitude()),
		                                static_cast<double>(loc.getLatitude()),
		                                mapRect);
		if (!mapRect.adjusted(4, 4, -4, -4).contains(p))
			continue;

		const QString name = loc.name;
		QRectF textRect(painter.fontMetrics().boundingRect(name).adjusted(-2, -1, 2, 1));
		textRect.translate(p + QPointF(5., -13.));
		bool overlaps = false;
		for (const QRectF& used : std::as_const(usedRects))
		{
			if (used.intersects(textRect))
			{
				overlaps = true;
				break;
			}
		}
		if (overlaps || !mapRect.contains(textRect))
			continue;

		painter.setPen(QPen(Qt::white, 3.0));
		painter.drawPoint(p);
		painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, name);
		painter.setPen(QPen(QColor(170, 0, 0), 1.2));
		painter.drawPoint(p);
		painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, name);
		usedRects << textRect.adjusted(-3, -2, 3, 2);
		++drawn;
	}
	painter.restore();
}

void SunriseSunsetMapWidget::drawGeographicGrid(QPainter& painter, const QRectF& mapRect) const
{
	painter.save();
	painter.setClipRect(mapRect);
	painter.setRenderHint(QPainter::Antialiasing);

	const QPen ordinaryPen(QColor(0, 0, 0, 70), 0.8);
	const QPen equatorPen(QColor(0, 0, 0, 115), 1.1);
	const QPen specialPen(QColor(120, 82, 0, 145), 1.2, Qt::DashLine);
	const int stepDeg = longitudeSpanDeg > 120.0 ? 30 : (longitudeSpanDeg > 50.0 ? 15 : 5);

	const double latitudeSpanDeg = longitudeSpanDeg * mapRect.height() / mapRect.width();
	const double minLat = qMax(-90.0, centerLatitudeDeg - latitudeSpanDeg / 2.0);
	const double maxLat = qMin(90.0, centerLatitudeDeg + latitudeSpanDeg / 2.0);
	const int firstLat = static_cast<int>(qCeil(minLat / stepDeg)) * stepDeg;
	for (int lat = firstLat; lat <= maxLat; lat += stepDeg)
		drawLatitudeLine(painter, mapRect, lat, lat == 0 ? equatorPen : ordinaryPen);

	const double leftLon = centerLongitudeDeg - longitudeSpanDeg / 2.0;
	const double rightLon = centerLongitudeDeg + longitudeSpanDeg / 2.0;
	const int firstLon = static_cast<int>(qCeil(leftLon / stepDeg)) * stepDeg;
	for (int lon = firstLon; lon <= rightLon; lon += stepDeg)
		drawLongitudeLine(painter, mapRect, lon, ordinaryPen);

	const double obliquity = axialTiltDeg();
	const double polarCircle = 90.0 - obliquity;
	drawLatitudeLine(painter, mapRect, obliquity, specialPen);
	drawLatitudeLine(painter, mapRect, -obliquity, specialPen);
	drawLatitudeLine(painter, mapRect, polarCircle, specialPen);
	drawLatitudeLine(painter, mapRect, -polarCircle, specialPen);

	painter.restore();
}

void SunriseSunsetMapWidget::drawLatitudeLine(QPainter& painter, const QRectF& mapRect,
                                              double latitudeDeg, const QPen& pen) const
{
	painter.setPen(pen);
	QVector<QPointF> points;
	const int segments = qBound(24, qRound(mapRect.width() / 24.0), 160);
	points.reserve(segments + 1);
	for (int i = 0; i <= segments; ++i)
	{
		const double lon = centerLongitudeDeg - longitudeSpanDeg / 2.0 +
		                   static_cast<double>(i) * longitudeSpanDeg / segments;
		points << lonLatToPoint(normalizeLongitudeDeg(lon), latitudeDeg, mapRect);
	}
	painter.drawPolyline(points.constData(), points.size());
}

void SunriseSunsetMapWidget::drawLongitudeLine(QPainter& painter, const QRectF& mapRect,
                                               double longitudeDeg, const QPen& pen) const
{
	painter.setPen(pen);
	QVector<QPointF> points;
	const int segments = qBound(18, qRound(mapRect.height() / 18.0), 120);
	points.reserve(segments + 1);
	const double latitudeSpanDeg = longitudeSpanDeg * mapRect.height() / mapRect.width();
	const double minLat = qMax(-90.0, centerLatitudeDeg - latitudeSpanDeg / 2.0);
	const double maxLat = qMin(90.0, centerLatitudeDeg + latitudeSpanDeg / 2.0);
	for (int i = 0; i <= segments; ++i)
	{
		const double lat = maxLat - static_cast<double>(i) * (maxLat - minLat) / segments;
		points << lonLatToPoint(normalizeLongitudeDeg(longitudeDeg), lat, mapRect);
	}
	painter.drawPolyline(points.constData(), points.size());
}

void SunriseSunsetMapWidget::updateDateCache()
{
	if (!core)
		return;

	currentJD = core->getJD();
	utcOffsetHours = core->getUTCOffset(currentJD);
	int hour = 0;
	int minute = 0;
	int second = 0;
	int millis = 0;
	StelUtils::getDateTimeFromJulianDay(currentJD + utcOffsetHours / 24.0,
	                                    &year, &month, &day, &hour, &minute, &second, &millis);
	const QDate date(year, month, day);
	dayOfYear = date.isValid() ? date.dayOfYear() : 1;
}

void SunriseSunsetMapWidget::constrainView()
{
	const double latitudeSpanDeg = longitudeSpanDeg * static_cast<double>(height()) / qMax(1, width());
	const double maxCenterLat = qMax(0.0, 90.0 - latitudeSpanDeg / 2.0);
	centerLatitudeDeg = qBound(-maxCenterLat, centerLatitudeDeg, maxCenterLat);
	centerLongitudeDeg = normalizeLongitudeDeg(centerLongitudeDeg);
}
