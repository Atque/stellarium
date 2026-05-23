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

#include "VisibilityMapDialog.hpp"
#include "StarVisibilityMapWidget.hpp"

#include "Dialog.hpp"
#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelObjectMgr.hpp"
#include "StelTranslator.hpp"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

VisibilityMapDialog::VisibilityMapDialog()
	: StelDialog("VisibilityMap")
	, titleBar(Q_NULLPTR)
	, starVisibilityMapWidget(Q_NULLPTR)
	, starVisGridCheckBox(Q_NULLPTR)
	, starVisCitiesCheckBox(Q_NULLPTR)
	, setLocationCheckBox(Q_NULLPTR)
	, calculateButton(Q_NULLPTR)
	, starVisLocationButton(Q_NULLPTR)
	, starVisResetButton(Q_NULLPTR)
	, starVisAltLabel(Q_NULLPTR)
	, core(Q_NULLPTR)
{
}

VisibilityMapDialog::~VisibilityMapDialog()
{
}

void VisibilityMapDialog::retranslate()
{
	if (!dialog)
		return;

	// Note: q_() strings will only be translated once this plugin is added
	// to Stellarium's po/stellarium-visibilitymap/ translation catalogue.
	// Until then, retranslate() ensures the UI is consistent after a language
	// change even if strings remain in English.
	titleBar->setTitle(q_("Visibility Map"));

	// Shared checkbox
	if (setLocationCheckBox)
		setLocationCheckBox->setText(q_("Click map to set location"));

	// ── Object visibility controls ───────────────────────────────────────────
	if (calculateButton)       calculateButton->setText(q_("Calculate"));
	if (starVisGridCheckBox)   starVisGridCheckBox->setText(q_("Grid"));
	if (starVisCitiesCheckBox) starVisCitiesCheckBox->setText(q_("Cities"));
	if (starVisLocationButton) starVisLocationButton->setText(q_("Current location"));
	if (starVisResetButton)    starVisResetButton->setText(q_("World"));
	if (starVisAltLabel)       starVisAltLabel->setText(q_("Min. altitude:"));

	// Invalidate widget cache — translated strings are baked into it.
	if (starVisibilityMapWidget) starVisibilityMapWidget->invalidateCache();
}

void VisibilityMapDialog::createDialogContent()
{
	core = StelApp::getInstance().getCore();

	dialog->setMinimumSize(760, 520);

	QVBoxLayout* mainLayout = new QVBoxLayout(dialog);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->setSpacing(0);

	titleBar = new TitleBar(dialog);
	titleBar->setObjectName(QStringLiteral("titleBar"));
	titleBar->setTitle(q_("Visibility Map"));
	mainLayout->addWidget(titleBar);
	connect(titleBar, &TitleBar::closeClicked, this, &StelDialog::close);
	connect(titleBar, SIGNAL(movedTo(QPoint)), this, SLOT(handleMovedTo(QPoint)));

	// ── Body layout (single Object visibility view) ──────────────────────────
	QWidget* bodyWidget = new QWidget(dialog);
	QVBoxLayout* bodyLayout = new QVBoxLayout(bodyWidget);
	bodyLayout->setContentsMargins(6, 6, 6, 6);
	bodyLayout->setSpacing(6);
	mainLayout->addWidget(bodyWidget, 1);

	// World-map widget
	starVisibilityMapWidget = new StarVisibilityMapWidget(bodyWidget);
	starVisibilityMapWidget->setMinimumSize(720, 360);
	starVisibilityMapWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	bodyLayout->addWidget(starVisibilityMapWidget, 1);

	// ── Controls row ─────────────────────────────────────────────────────────
	QHBoxLayout* starVisControls = new QHBoxLayout;
	starVisControls->setSpacing(6);
	bodyLayout->addLayout(starVisControls);

	// "Calculate" — enabled only when Stellarium has a selected object.
	calculateButton = new QPushButton(q_("Calculate"), bodyWidget);
	calculateButton->setToolTip(
	        q_("Compute and display visibility for the selected object at the current simulation date"));
	calculateButton->setEnabled(starVisibilityMapWidget->hasSelection());
	starVisControls->addWidget(calculateButton);
	connect(calculateButton, &QPushButton::clicked,
	        starVisibilityMapWidget, &StarVisibilityMapWidget::calculateVisibility);
	connect(calculateButton, &QPushButton::clicked,
	        this, &VisibilityMapDialog::onCalculateStarVisibility);

	// Grid checkbox
	starVisGridCheckBox = new QCheckBox(q_("Grid"), bodyWidget);
	starVisGridCheckBox->setToolTip(q_("Show geographic grid"));
	starVisControls->addWidget(starVisGridCheckBox);
	connect(starVisGridCheckBox, &QCheckBox::toggled,
	        starVisibilityMapWidget, &StarVisibilityMapWidget::setFlagShowGrid);

	starVisCitiesCheckBox = new QCheckBox(q_("Cities"), bodyWidget);
	starVisCitiesCheckBox->setChecked(true);
	starVisCitiesCheckBox->setToolTip(q_("Show city labels when zoomed in"));
	starVisControls->addWidget(starVisCitiesCheckBox);
	connect(starVisCitiesCheckBox, &QCheckBox::toggled,
	        starVisibilityMapWidget, &StarVisibilityMapWidget::setFlagShowCities);

	// Zoom to current location
	starVisLocationButton = new QPushButton(q_("Current location"), bodyWidget);
	starVisControls->addWidget(starVisLocationButton);
	connect(starVisLocationButton, &QPushButton::clicked,
	        this, &VisibilityMapDialog::zoomStarVisibilityToCurrentLocation);

	// Reset / world view
	starVisResetButton = new QPushButton(q_("World"), bodyWidget);
	starVisControls->addWidget(starVisResetButton);
	connect(starVisResetButton, &QPushButton::clicked,
	        this, &VisibilityMapDialog::resetStarVisibilityView);

	// Minimum-altitude spinbox
	starVisAltLabel = new QLabel(q_("Min. altitude:"), bodyWidget);
	starVisControls->addWidget(starVisAltLabel);
	QSpinBox* goodAltSpin = new QSpinBox(bodyWidget);
	goodAltSpin->setRange(1, 89);
	goodAltSpin->setValue(5);
	goodAltSpin->setSuffix(QStringLiteral("°"));
	goodAltSpin->setToolTip(q_("Minimum altitude for the \"good visibility\" dash-dot line"));
	starVisControls->addWidget(goodAltSpin);
	connect(goodAltSpin, QOverload<int>::of(&QSpinBox::valueChanged),
	        starVisibilityMapWidget, &StarVisibilityMapWidget::setGoodVisibilityAltitude);

	starVisControls->addStretch(1);

	// "Click to set location" checkbox — when ticked, clicking the map
	// moves the Stellarium observer to that geographic position.
	setLocationCheckBox = new QCheckBox(q_("Click map to set location"), bodyWidget);
	setLocationCheckBox->setToolTip(
	        q_("When checked, clicking anywhere on the map instantly moves the "
	           "Stellarium observer to that geographic location."));
	setLocationCheckBox->setChecked(false);
	starVisControls->addWidget(setLocationCheckBox);

	// Hook up the shared "click to set location" checkbox to the map widget.
	connect(setLocationCheckBox, &QCheckBox::toggled,
	        starVisibilityMapWidget, &StarVisibilityMapWidget::setFlagSetLocationOnClick);

	// ── Connections for object-selection changes ──────────────────────────
	// When the user clicks a different object, only the button enable-state
	// changes. The map itself is only redrawn when the user clicks Calculate.
	connect(&StelApp::getInstance().getStelObjectMgr(),
	        SIGNAL(selectedObjectChanged(StelModule::StelModuleSelectAction)),
	        starVisibilityMapWidget,
	        SLOT(onSelectionChanged()));
	connect(starVisibilityMapWidget, &StarVisibilityMapWidget::selectionStateChanged,
	        this, &VisibilityMapDialog::onStarVisSelectionChanged);
	connect(starVisibilityMapWidget, &StarVisibilityMapWidget::solarSystemObjectSelected,
	        this, &VisibilityMapDialog::onSolarSystemObjectSelected);

	// ── Global signals ────────────────────────────────────────────────────
	connect(&StelApp::getInstance(), SIGNAL(languageChanged()), this, SLOT(retranslate()));

	retranslate();
}

// ── Object visibility slots ───────────────────────────────────────────────

void VisibilityMapDialog::onSolarSystemObjectSelected()
{
	// The user clicked Calculate on a solar system object.
	// The world map view still works (it only needs declination),
	// so this slot is currently a no-op. Kept as a hook in case future
	// features need to react to solar-system selection (e.g. disabling
	// controls that only make sense for stars).
}

void VisibilityMapDialog::onCalculateStarVisibility()
{
	// Currently a no-op — kept as a hook for future post-calculate actions.
	// (Previously this forwarded data to the now-removed calendar widget.)
}

void VisibilityMapDialog::onStarVisSelectionChanged(bool objectAvailable)
{
	if (calculateButton)
		calculateButton->setEnabled(objectAvailable);
}

void VisibilityMapDialog::resetStarVisibilityView()
{
	if (starVisibilityMapWidget)
		starVisibilityMapWidget->resetView();
}

void VisibilityMapDialog::zoomStarVisibilityToCurrentLocation()
{
	if (starVisibilityMapWidget)
		starVisibilityMapWidget->zoomToCurrentLocation();
}
