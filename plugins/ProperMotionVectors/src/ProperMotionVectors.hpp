/*
 * Proper Motion Vectors plug-in for Stellarium
 *
 * Copyright (C) 2026 Atque
 * Author: Atque, assisted by Codex
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef PROPERMOTIONVECTORS_HPP
#define PROPERMOTIONVECTORS_HPP

#include "StelModule.hpp"
#include "StelProjectorType.hpp"
#include "StelSphereGeometry.hpp"
#include "StarMgr.hpp"
#include "VecMath.hpp"

#include <QObject>
#include <QVector>
#include <limits>

class QSettings;

#ifndef NO_GUI
class ProperMotionVectorsDialog;
class StelButton;
#endif

class ProperMotionVectors : public StelModule
{
	Q_OBJECT
	Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
	Q_PROPERTY(bool magnitudeFilterEnabled READ isMagnitudeFilterEnabled WRITE setMagnitudeFilterEnabled NOTIFY magnitudeFilterEnabledChanged)
	Q_PROPERTY(bool properMotionFilterEnabled READ isProperMotionFilterEnabled WRITE setProperMotionFilterEnabled NOTIFY properMotionFilterEnabledChanged)
	Q_PROPERTY(bool unknownRadialVelocityVisible READ isUnknownRadialVelocityVisible WRITE setUnknownRadialVelocityVisible NOTIFY unknownRadialVelocityVisibleChanged)
	Q_PROPERTY(double limitMagnitude READ getLimitMagnitude WRITE setLimitMagnitude NOTIFY limitMagnitudeChanged)
	Q_PROPERTY(double minimumProperMotion READ getMinimumProperMotion WRITE setMinimumProperMotion NOTIFY minimumProperMotionChanged)
	Q_PROPERTY(double vectorLengthScale READ getVectorLengthScale WRITE setVectorLengthScale NOTIFY vectorLengthScaleChanged)
	Q_PROPERTY(double radialVelocitySaturation READ getRadialVelocitySaturation WRITE setRadialVelocitySaturation NOTIFY radialVelocitySaturationChanged)

public:
	ProperMotionVectors();
	~ProperMotionVectors() override;

	void init() override;
	void deinit() override;
	void update(double deltaTime) override;
	void draw(StelCore* core) override;
	double getCallOrder(StelModuleActionName actionName) const override;
	bool configureGui(bool show) override;

	bool isEnabled() const { return enabled; }
	bool isMagnitudeFilterEnabled() const { return magnitudeFilterEnabled; }
	bool isProperMotionFilterEnabled() const { return properMotionFilterEnabled; }
	bool isUnknownRadialVelocityVisible() const { return unknownRadialVelocityVisible; }
	double getLimitMagnitude() const { return limitMagnitude; }
	double getMinimumProperMotion() const { return minimumProperMotion; }
	double getVectorLengthScale() const { return vectorLengthScale; }
	double getRadialVelocitySaturation() const { return radialVelocitySaturation; }

public slots:
	void setEnabled(bool value);
	void setMagnitudeFilterEnabled(bool value);
	void setProperMotionFilterEnabled(bool value);
	void setUnknownRadialVelocityVisible(bool value);
	void setLimitMagnitude(double value);
	void setMinimumProperMotion(double value);
	void setVectorLengthScale(double value);
	void setRadialVelocitySaturation(double value);
	void restoreDefaults();

signals:
	void enabledChanged(bool value);
	void magnitudeFilterEnabledChanged(bool value);
	void properMotionFilterEnabledChanged(bool value);
	void unknownRadialVelocityVisibleChanged(bool value);
	void limitMagnitudeChanged(double value);
	void minimumProperMotionChanged(double value);
	void vectorLengthScaleChanged(double value);
	void radialVelocitySaturationChanged(double value);

private:
	void loadSettings();
	void saveSettings() const;
#ifndef NO_GUI
	void ensureToolbarButton();
#endif
	const QVector<StarKinematics>& updateVectorCache(StelCore* core, const SphericalRegionP& collectionRegion);
	void appendStarVector(QVector<Vec2f>& vertices, QVector<Vec3f>& colors, const SphericalRegionP& viewport, const StelProjectorP& projector, const StarKinematics& kinematics) const;
	static Vec3f colorForRadialVelocity(double radialVelocity, double saturation);

	QSettings* conf = nullptr;
	StarMgr* starMgr = nullptr;
	QVector<StarKinematics> cachedVectors;
	SphericalRegionP cachedRegion;
	double cachedJDE = -std::numeric_limits<double>::max();
	double cachedLimitMagnitude = 0.0;
	double cachedMinimumProperMotion = 0.0;
	double cachedParallaxFactor = 0.0;
	bool cachedMagnitudeFilterEnabled = false;
	bool cachedProperMotionFilterEnabled = false;
	bool cachedUnknownRadialVelocityVisible = true;
	bool cachedUseParallax = false;
	bool cachedUseAberration = false;

	bool enabled = false;
	bool magnitudeFilterEnabled = true;
	bool properMotionFilterEnabled = true;
	bool unknownRadialVelocityVisible = true;
	double limitMagnitude = 6.0;
	double minimumProperMotion = 1.0;
	double vectorLengthScale = 40.0;
	double radialVelocitySaturation = 600.0;

#ifndef NO_GUI
	ProperMotionVectorsDialog* configDialog = nullptr;
	StelButton* toolbarButton = nullptr;
#endif
};

#include "StelPluginInterface.hpp"

class ProperMotionVectorsStelPluginInterface : public QObject, public StelPluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID StelPluginInterface_iid)
	Q_INTERFACES(StelPluginInterface)
public:
	StelModule* getStelModule() const override;
	StelPluginInfo getPluginInfo() const override;
};

#endif
