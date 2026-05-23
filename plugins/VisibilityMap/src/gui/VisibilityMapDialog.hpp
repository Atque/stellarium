/*
 * Visibility Map plug-in for Stellarium
 *
 * Copyright (C) 2026 Atque
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef VISIBILITYMAP_DIALOG_HPP
#define VISIBILITYMAP_DIALOG_HPP

#include "StelDialog.hpp"

class StarVisibilityMapWidget;
class QLabel;
class QCheckBox;
class QPushButton;
class TitleBar;
class StelCore;

class VisibilityMapDialog : public StelDialog
{
	Q_OBJECT

public:
	VisibilityMapDialog();
	~VisibilityMapDialog() override;

public slots:
	void retranslate() override;

protected:
	void createDialogContent() override;

private slots:
	void resetStarVisibilityView();
	void zoomStarVisibilityToCurrentLocation();
	void onStarVisSelectionChanged(bool objectAvailable);
	void onCalculateStarVisibility();       //!< Re-enables the Calculate button after a successful calculation.
	void onSolarSystemObjectSelected();     //!< Called when Calculate is rejected for a solar system object.

private:
	TitleBar*                titleBar;
	StarVisibilityMapWidget* starVisibilityMapWidget;
	QCheckBox*               starVisGridCheckBox;
	QCheckBox*               starVisCitiesCheckBox;
	QCheckBox*               setLocationCheckBox;
	QPushButton*             calculateButton;
	QPushButton*             starVisLocationButton;
	QPushButton*             starVisResetButton;
	QLabel*                  starVisAltLabel;
	StelCore*                core;
};

#endif /* VISIBILITYMAP_DIALOG_HPP */
