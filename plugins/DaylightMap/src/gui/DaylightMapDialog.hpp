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

#ifndef DAYLIGHTMAP_DIALOG_HPP
#define DAYLIGHTMAP_DIALOG_HPP

#include "StelDialog.hpp"

class EarthShadowMapWidget;
class SunriseSunsetMapWidget;
class QLabel;
class QComboBox;
class QCheckBox;
class QPushButton;
class QTabWidget;
class TitleBar;
class StelCore;

class DaylightMapDialog : public StelDialog
{
	Q_OBJECT

public:
	DaylightMapDialog();
	~DaylightMapDialog() override;

public slots:
	void retranslate() override;

protected:
	void createDialogContent() override;
	bool eventFilter(QObject* object, QEvent* event) override;

private slots:
	void stepBackward();
	void stepForward();
	void setToCurrentSystemTime();
	void refresh();
	void setShowGeographicGrid(bool show);
	void resetIsolineView();
	void zoomIsolineViewToCurrentLocation();
	void syncIsolineFromCore();
	void previousIsolineDay();
	void nextIsolineDay();

private:
	double currentStepDays() const;
	void stepBy(int multiplier);
	void updateLabels();

	TitleBar* titleBar;
	EarthShadowMapWidget* mapWidget;
	SunriseSunsetMapWidget* isolineMapWidget;
	QTabWidget* tabWidget;
	QComboBox* stepCombo;
	QComboBox* isolineBodyCombo;
	QComboBox* isolineModeCombo;
	QCheckBox* twilightGridCheckBox;
	QCheckBox* isolineGridCheckBox;
	QLabel* currentTimeLabel;
	QLabel* stepValueLabel;
	StelCore* core;
};

#endif /* DAYLIGHTMAP_DIALOG_HPP */
