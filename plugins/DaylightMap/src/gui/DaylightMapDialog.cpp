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

#include "DaylightMapDialog.hpp"
#include "EarthShadowMapWidget.hpp"
#include "SunriseSunsetMapWidget.hpp"

#include "Dialog.hpp"
#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelTranslator.hpp"
#include "StelUtils.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace
{
enum StepType
{
	StepMinute = 0,
	StepHour,
	StepDay,
	StepCalendarMonth,
	StepCalendarYear
};
}

DaylightMapDialog::DaylightMapDialog()
	: StelDialog("DaylightMap")
	, titleBar(Q_NULLPTR)
	, mapWidget(Q_NULLPTR)
	, isolineMapWidget(Q_NULLPTR)
	, tabWidget(Q_NULLPTR)
	, stepCombo(Q_NULLPTR)
	, isolineBodyCombo(Q_NULLPTR)
	, isolineModeCombo(Q_NULLPTR)
	, twilightGridCheckBox(Q_NULLPTR)
	, isolineGridCheckBox(Q_NULLPTR)
	, currentTimeLabel(Q_NULLPTR)
	, stepValueLabel(Q_NULLPTR)
	, core(Q_NULLPTR)
{
}

DaylightMapDialog::~DaylightMapDialog()
{
}

void DaylightMapDialog::retranslate()
{
	if (!dialog)
		return;

	titleBar->setTitle(q_("Daylight Map"));

	const int idx = stepCombo->currentIndex();
	stepCombo->blockSignals(true);
	stepCombo->clear();
	stepCombo->addItem(q_("Minute"), StepMinute);
	stepCombo->addItem(q_("Hour"), StepHour);
	stepCombo->addItem(q_("Day"), StepDay);
	stepCombo->addItem(q_("Calendar month"), StepCalendarMonth);
	stepCombo->addItem(q_("Calendar year"), StepCalendarYear);
	stepCombo->setCurrentIndex(qBound(0, idx, stepCombo->count() - 1));
	stepCombo->blockSignals(false);

	updateLabels();
}

void DaylightMapDialog::createDialogContent()
{
	core = StelApp::getInstance().getCore();

	dialog->setMinimumSize(760, 520);
	dialog->installEventFilter(this);
	dialog->setFocusPolicy(Qt::StrongFocus);

	QVBoxLayout* mainLayout = new QVBoxLayout(dialog);
	mainLayout->setContentsMargins(8, 8, 8, 8);
	mainLayout->setSpacing(6);

	titleBar = new TitleBar(dialog);
	mainLayout->addWidget(titleBar);
	connect(titleBar, &TitleBar::closeClicked, this, &StelDialog::close);
	connect(titleBar, SIGNAL(movedTo(QPoint)), this, SLOT(handleMovedTo(QPoint)));

	tabWidget = new QTabWidget(dialog);
	tabWidget->setTabPosition(QTabWidget::South);
	mainLayout->addWidget(tabWidget, 1);

	QWidget* twilightTab = new QWidget(tabWidget);
	QVBoxLayout* twilightLayout = new QVBoxLayout(twilightTab);
	twilightLayout->setContentsMargins(0, 0, 0, 0);
	twilightLayout->setSpacing(6);

	mapWidget = new EarthShadowMapWidget(dialog);
	mapWidget->setMinimumSize(720, 360);
	mapWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	twilightLayout->addWidget(mapWidget, 1);

	QHBoxLayout* timeLayout = new QHBoxLayout;
	timeLayout->setSpacing(6);
	twilightLayout->addLayout(timeLayout);

	QLabel* stepLabel = new QLabel(q_("Step"), dialog);
	timeLayout->addWidget(stepLabel);

	stepCombo = new QComboBox(dialog);
	timeLayout->addWidget(stepCombo);
	connect(stepCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(refresh()));

	QToolButton* backButton = new QToolButton(dialog);
	backButton->setText(QStringLiteral("<"));
	backButton->setToolTip(q_("Step backward"));
	backButton->setAutoRepeat(true);
	backButton->setAutoRepeatDelay(350);
	backButton->setAutoRepeatInterval(70);
	timeLayout->addWidget(backButton);
	connect(backButton, &QToolButton::clicked, this, &DaylightMapDialog::stepBackward);

	stepValueLabel = new QLabel(dialog);
	stepValueLabel->setAlignment(Qt::AlignCenter);
	stepValueLabel->setMinimumWidth(120);
	timeLayout->addWidget(stepValueLabel, 1);

	QToolButton* forwardButton = new QToolButton(dialog);
	forwardButton->setText(QStringLiteral(">"));
	forwardButton->setToolTip(q_("Step forward"));
	forwardButton->setAutoRepeat(true);
	forwardButton->setAutoRepeatDelay(350);
	forwardButton->setAutoRepeatInterval(70);
	timeLayout->addWidget(forwardButton);
	connect(forwardButton, &QToolButton::clicked, this, &DaylightMapDialog::stepForward);

	QPushButton* nowButton = new QPushButton(q_("Now"), dialog);
	timeLayout->addWidget(nowButton);
	connect(nowButton, &QPushButton::clicked, this, &DaylightMapDialog::setToCurrentSystemTime);

	twilightGridCheckBox = new QCheckBox(q_("Grid"), dialog);
	twilightGridCheckBox->setToolTip(q_("Show geographic grid, tropics, and polar circles"));
	timeLayout->addWidget(twilightGridCheckBox);
	connect(twilightGridCheckBox, &QCheckBox::toggled, this, &DaylightMapDialog::setShowGeographicGrid);

	QHBoxLayout* labelLayout = new QHBoxLayout;
	labelLayout->setSpacing(8);
	twilightLayout->addLayout(labelLayout);

	currentTimeLabel = new QLabel(dialog);
	currentTimeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
	labelLayout->addWidget(currentTimeLabel, 1);

	tabWidget->addTab(twilightTab, q_("Twilight zones"));

	QWidget* isolineTab = new QWidget(tabWidget);
	QVBoxLayout* isolineLayout = new QVBoxLayout(isolineTab);
	isolineLayout->setContentsMargins(0, 0, 0, 0);
	isolineLayout->setSpacing(6);

	isolineMapWidget = new SunriseSunsetMapWidget(isolineTab);
	isolineMapWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	isolineLayout->addWidget(isolineMapWidget, 1);

	QHBoxLayout* isolineControls = new QHBoxLayout;
	isolineControls->setSpacing(6);
	isolineLayout->addLayout(isolineControls);

	isolineControls->addWidget(new QLabel(q_("Body"), isolineTab));
	isolineBodyCombo = new QComboBox(isolineTab);
	isolineBodyCombo->addItem(q_("Sun"), SunriseSunsetMapWidget::Sun);
	isolineBodyCombo->addItem(q_("Moon"), SunriseSunsetMapWidget::Moon);
	isolineControls->addWidget(isolineBodyCombo);
	connect(isolineBodyCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(refresh()));

	isolineControls->addWidget(new QLabel(q_("Event"), isolineTab));
	isolineModeCombo = new QComboBox(isolineTab);
	isolineModeCombo->addItem(q_("Rise"), SunriseSunsetMapWidget::Sunrise);
	isolineModeCombo->addItem(q_("Set"), SunriseSunsetMapWidget::Sunset);
	isolineControls->addWidget(isolineModeCombo);
	connect(isolineModeCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(refresh()));

	QPushButton* syncButton = new QPushButton(q_("Sync"), isolineTab);
	syncButton->setToolTip(q_("Sync time and location"));
	isolineControls->addWidget(syncButton);
	connect(syncButton, &QPushButton::clicked, this, &DaylightMapDialog::syncIsolineFromCore);

	isolineGridCheckBox = new QCheckBox(q_("Grid"), isolineTab);
	isolineGridCheckBox->setToolTip(q_("Show geographic grid, tropics, and polar circles"));
	isolineControls->addWidget(isolineGridCheckBox);
	connect(isolineGridCheckBox, &QCheckBox::toggled, this, &DaylightMapDialog::setShowGeographicGrid);

	QToolButton* previousDayButton = new QToolButton(isolineTab);
	previousDayButton->setText(QStringLiteral("<"));
	previousDayButton->setToolTip(q_("Previous day"));
	previousDayButton->setAutoRepeat(true);
	previousDayButton->setAutoRepeatDelay(350);
	previousDayButton->setAutoRepeatInterval(90);
	isolineControls->addWidget(previousDayButton);
	connect(previousDayButton, &QToolButton::clicked, this, &DaylightMapDialog::previousIsolineDay);

	QLabel* dayLabel = new QLabel(q_("Day"), isolineTab);
	dayLabel->setAlignment(Qt::AlignCenter);
	isolineControls->addWidget(dayLabel);

	QToolButton* nextDayButton = new QToolButton(isolineTab);
	nextDayButton->setText(QStringLiteral(">"));
	nextDayButton->setToolTip(q_("Next day"));
	nextDayButton->setAutoRepeat(true);
	nextDayButton->setAutoRepeatDelay(350);
	nextDayButton->setAutoRepeatInterval(90);
	isolineControls->addWidget(nextDayButton);
	connect(nextDayButton, &QToolButton::clicked, this, &DaylightMapDialog::nextIsolineDay);

	QPushButton* currentLocationButton = new QPushButton(q_("Current location"), isolineTab);
	isolineControls->addWidget(currentLocationButton);
	connect(currentLocationButton, &QPushButton::clicked, this, &DaylightMapDialog::zoomIsolineViewToCurrentLocation);

	QPushButton* resetViewButton = new QPushButton(q_("World"), isolineTab);
	isolineControls->addWidget(resetViewButton);
	connect(resetViewButton, &QPushButton::clicked, this, &DaylightMapDialog::resetIsolineView);
	isolineControls->addStretch(1);

	tabWidget->addTab(isolineTab, q_("Rise/set isolines"));

	connect(&StelApp::getInstance(), SIGNAL(languageChanged()), this, SLOT(retranslate()));

	retranslate();
	refresh();
}

bool DaylightMapDialog::eventFilter(QObject* object, QEvent* event)
{
	if (object == dialog && event->type() == QEvent::KeyPress)
	{
		QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
		switch (keyEvent->key())
		{
			case Qt::Key_Left:
				stepBy(-1);
				return true;
			case Qt::Key_Right:
				stepBy(1);
				return true;
			case Qt::Key_PageUp:
				stepBy(10);
				return true;
			case Qt::Key_PageDown:
				stepBy(-10);
				return true;
			default:
				break;
		}
	}

	return StelDialog::eventFilter(object, event);
}

double DaylightMapDialog::currentStepDays() const
{
	switch (stepCombo->currentData().toInt())
	{
		case StepMinute:
			return 1.0 / 1440.0;
		case StepHour:
			return 1.0 / 24.0;
		case StepDay:
			return 1.0;
		default:
			return 0.;
	}
}

void DaylightMapDialog::stepBackward()
{
	stepBy(-1);
}

void DaylightMapDialog::stepForward()
{
	stepBy(1);
}

void DaylightMapDialog::setToCurrentSystemTime()
{
	core->setJD(StelUtils::getJDFromSystem());
	refresh();
}

void DaylightMapDialog::stepBy(int multiplier)
{
	switch (stepCombo->currentData().toInt())
	{
		case StepCalendarMonth:
			for (int i = 0; i < qAbs(multiplier); ++i)
			{
				if (multiplier > 0)
					core->addCalendarMonth();
				else
					core->subtractCalendarMonth();
			}
			break;
		case StepCalendarYear:
			for (int i = 0; i < qAbs(multiplier); ++i)
			{
				if (multiplier > 0)
					core->addCalendarYear();
				else
					core->subtractCalendarYear();
			}
			break;
		default:
			core->setJD(core->getJD() + multiplier * currentStepDays());
			break;
	}

	refresh();
}

void DaylightMapDialog::refresh()
{
	if (mapWidget)
	{
		mapWidget->updateFromCore();
		mapWidget->setFlagShowGrid(twilightGridCheckBox && twilightGridCheckBox->isChecked());
	}
	if (isolineMapWidget)
	{
		isolineMapWidget->setBodyMode(isolineBodyCombo->currentData().toInt());
		isolineMapWidget->setEventMode(isolineModeCombo->currentData().toInt());
		isolineMapWidget->setFlagShowGrid(isolineGridCheckBox && isolineGridCheckBox->isChecked());
		isolineMapWidget->updateFromCore();
	}
	updateLabels();
}

void DaylightMapDialog::setShowGeographicGrid(bool show)
{
	if (twilightGridCheckBox && twilightGridCheckBox->isChecked() != show)
	{
		twilightGridCheckBox->blockSignals(true);
		twilightGridCheckBox->setChecked(show);
		twilightGridCheckBox->blockSignals(false);
	}

	if (isolineGridCheckBox && isolineGridCheckBox->isChecked() != show)
	{
		isolineGridCheckBox->blockSignals(true);
		isolineGridCheckBox->setChecked(show);
		isolineGridCheckBox->blockSignals(false);
	}

	if (mapWidget)
		mapWidget->setFlagShowGrid(show);
	if (isolineMapWidget)
		isolineMapWidget->setFlagShowGrid(show);
}

void DaylightMapDialog::resetIsolineView()
{
	if (isolineMapWidget)
		isolineMapWidget->resetView();
}

void DaylightMapDialog::zoomIsolineViewToCurrentLocation()
{
	if (isolineMapWidget)
		isolineMapWidget->zoomToCurrentLocation();
}

void DaylightMapDialog::syncIsolineFromCore()
{
	if (mapWidget)
		mapWidget->updateFromCore();

	if (isolineMapWidget)
	{
		isolineMapWidget->setBodyMode(isolineBodyCombo->currentData().toInt());
		isolineMapWidget->setEventMode(isolineModeCombo->currentData().toInt());
		isolineMapWidget->updateFromCore();
		isolineMapWidget->zoomToCurrentLocation();
	}

	updateLabels();
}

void DaylightMapDialog::previousIsolineDay()
{
	if (isolineMapWidget)
	{
		isolineMapWidget->addDays(-1);
		refresh();
	}
}

void DaylightMapDialog::nextIsolineDay()
{
	if (isolineMapWidget)
	{
		isolineMapWidget->addDays(1);
		refresh();
	}
}

void DaylightMapDialog::updateLabels()
{
	if (!core || !currentTimeLabel || !stepValueLabel)
		return;

	currentTimeLabel->setText(q_("UTC") + QStringLiteral(": ") +
	                          StelUtils::julianDayToISO8601String(core->getJD(), false));
	stepValueLabel->setText(QStringLiteral("1 ") + stepCombo->currentText());
}
