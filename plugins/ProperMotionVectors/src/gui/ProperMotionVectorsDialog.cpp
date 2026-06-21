/*
 * Proper Motion Vectors plug-in settings dialog.
 *
 * Author: Atque, assisted by Codex
 */

#include "ProperMotionVectorsDialog.hpp"

#include "Dialog.hpp"
#include "ProperMotionVectors.hpp"
#include "StelApp.hpp"
#include "StelGui.hpp"
#include "StelModuleMgr.hpp"

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QPushButton>
#include <QTabWidget>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QWidget>

ProperMotionVectorsDialog::ProperMotionVectorsDialog(ProperMotionVectors* plugin)
	: StelDialog("ProperMotionVectors")
	, plugin(plugin)
{
}

void ProperMotionVectorsDialog::retranslate()
{
	if (!dialog)
		return;

	titleBar->setTitle(q_("Proper Motion Vectors"));
	tabWidget->setTabText(0, qc_("Settings", "tab in plugin windows"));
	tabWidget->setTabText(1, qc_("About", "tab in plugin windows"));
	enabledCheckBox->setText(q_("Show proper motion vectors"));
	magnitudeFilterCheckBox->setText(q_("Use magnitude limit"));
	properMotionFilterCheckBox->setText(q_("Use minimum proper motion"));
	unknownRadialVelocityCheckBox->setText(q_("Show stars without radial velocity"));
	limitMagnitudeSpinBox->setPrefix(QString());
	minimumProperMotionSpinBox->setSuffix(QString(" %1").arg(qc_("arcsec/yr", "proper motion")));
	vectorLengthScaleSpinBox->setSuffix(QString(" %1").arg(qc_("px per arcsec/yr", "proper motion vector scale")));
	radialVelocitySaturationSpinBox->setSuffix(QString(" %1").arg(qc_("km/s", "speed")));

	QString html = QStringLiteral("<html><head></head><body>");
	html += QStringLiteral("<h2>") + q_("Proper Motion Vectors plug-in") + QStringLiteral("</h2>");
	html += QStringLiteral("<table class='layout' width='90%'>");
	html += QStringLiteral("<tr><td><strong>") + q_("Version") + QStringLiteral(":</strong></td><td>") + PROPERMOTIONVECTORS_PLUGIN_VERSION + QStringLiteral("</td></tr>");
	html += QStringLiteral("<tr><td><strong>") + q_("License") + QStringLiteral(":</strong></td><td>") + PROPERMOTIONVECTORS_PLUGIN_LICENSE + QStringLiteral("</td></tr>");
	html += QStringLiteral("<tr><td><strong>") + q_("Author") + QStringLiteral(":</strong></td><td>Atque, assisted by Codex</td></tr></table>");
	html += QStringLiteral("<p>") + q_("This plug-in displays date-aware proper motion vectors for stars. Vector length represents total angular proper motion, while color represents radial velocity: blue for approaching stars and red for receding stars.") + QStringLiteral("</p>");
	html += QStringLiteral("<p>") + q_("Stars without known radial velocity are shown in yellow and may be hidden. Magnitude and minimum proper-motion filters can be used to reduce the number of displayed vectors.") + QStringLiteral("</p>");
	html += StelApp::getInstance().getModuleMgr().getStandardSupportLinksInfo("Proper Motion Vectors plugin");
	html += QStringLiteral("</body></html>");
	aboutTextBrowser->setHtml(html);
}

void ProperMotionVectorsDialog::createDialogContent()
{
	QVBoxLayout* mainLayout = new QVBoxLayout(dialog);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->setSpacing(0);
	titleBar = new TitleBar(dialog);
	mainLayout->addWidget(titleBar);

	tabWidget = new QTabWidget(dialog);
	tabWidget->setDocumentMode(false);
	mainLayout->addWidget(tabWidget);

	QWidget* settingsTab = new QWidget(tabWidget);
	tabWidget->addTab(settingsTab, QString());
	QVBoxLayout* contentLayout = new QVBoxLayout(settingsTab);

	QWidget* aboutTab = new QWidget(tabWidget);
	tabWidget->addTab(aboutTab, QString());
	QVBoxLayout* aboutLayout = new QVBoxLayout(aboutTab);
	aboutTextBrowser = new QTextBrowser(aboutTab);
	aboutTextBrowser->setOpenExternalLinks(true);
	aboutLayout->addWidget(aboutTextBrowser);

	StelGui* gui = dynamic_cast<StelGui*>(StelApp::getInstance().getGui());
	if (gui)
		aboutTextBrowser->document()->setDefaultStyleSheet(gui->getStelStyle().htmlStyleSheet);

	connect(&StelApp::getInstance(), SIGNAL(languageChanged()), this, SLOT(retranslate()));
	connect(titleBar, &TitleBar::closeClicked, this, &StelDialog::close);
	connect(titleBar, SIGNAL(movedTo(QPoint)), this, SLOT(handleMovedTo(QPoint)));

	enabledCheckBox = new QCheckBox(settingsTab);
	contentLayout->addWidget(enabledCheckBox);

	QFormLayout* formLayout = new QFormLayout();
	contentLayout->addLayout(formLayout);

	magnitudeFilterCheckBox = new QCheckBox(settingsTab);
	formLayout->addRow(QString(), magnitudeFilterCheckBox);

	limitMagnitudeSpinBox = new QDoubleSpinBox(settingsTab);
	limitMagnitudeSpinBox->setRange(-2.0, 25.0);
	limitMagnitudeSpinBox->setDecimals(1);
	limitMagnitudeSpinBox->setSingleStep(0.5);
	formLayout->addRow(q_("Limit magnitude"), limitMagnitudeSpinBox);

	properMotionFilterCheckBox = new QCheckBox(settingsTab);
	formLayout->addRow(QString(), properMotionFilterCheckBox);

	minimumProperMotionSpinBox = new QDoubleSpinBox(settingsTab);
	minimumProperMotionSpinBox->setRange(0.0, 10000.0);
	minimumProperMotionSpinBox->setDecimals(3);
	minimumProperMotionSpinBox->setSingleStep(0.1);
	formLayout->addRow(q_("Minimum proper motion"), minimumProperMotionSpinBox);

	unknownRadialVelocityCheckBox = new QCheckBox(settingsTab);
	formLayout->addRow(QString(), unknownRadialVelocityCheckBox);

	vectorLengthScaleSpinBox = new QDoubleSpinBox(settingsTab);
	vectorLengthScaleSpinBox->setRange(1.0, 1000.0);
	vectorLengthScaleSpinBox->setDecimals(1);
	vectorLengthScaleSpinBox->setSingleStep(5.0);
	formLayout->addRow(q_("Vector length"), vectorLengthScaleSpinBox);

	radialVelocitySaturationSpinBox = new QDoubleSpinBox(settingsTab);
	radialVelocitySaturationSpinBox->setRange(1.0, 3000.0);
	radialVelocitySaturationSpinBox->setDecimals(0);
	radialVelocitySaturationSpinBox->setSingleStep(25.0);
	formLayout->addRow(q_("Radial velocity color scale"), radialVelocitySaturationSpinBox);

	QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::RestoreDefaults | QDialogButtonBox::Close, settingsTab);
	contentLayout->addWidget(buttons);
	connect(buttons->button(QDialogButtonBox::Close), &QPushButton::clicked, this, &StelDialog::close);
	connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, plugin, &ProperMotionVectors::restoreDefaults);

	connect(enabledCheckBox, &QCheckBox::toggled, plugin, &ProperMotionVectors::setEnabled);
	connect(magnitudeFilterCheckBox, &QCheckBox::toggled, plugin, &ProperMotionVectors::setMagnitudeFilterEnabled);
	connect(properMotionFilterCheckBox, &QCheckBox::toggled, plugin, &ProperMotionVectors::setProperMotionFilterEnabled);
	connect(unknownRadialVelocityCheckBox, &QCheckBox::toggled, plugin, &ProperMotionVectors::setUnknownRadialVelocityVisible);
	connect(limitMagnitudeSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), plugin, &ProperMotionVectors::setLimitMagnitude);
	connect(minimumProperMotionSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), plugin, &ProperMotionVectors::setMinimumProperMotion);
	connect(vectorLengthScaleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), plugin, &ProperMotionVectors::setVectorLengthScale);
	connect(radialVelocitySaturationSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), plugin, &ProperMotionVectors::setRadialVelocitySaturation);

	connect(plugin, &ProperMotionVectors::enabledChanged, this, &ProperMotionVectorsDialog::syncFromPlugin);
	connect(plugin, &ProperMotionVectors::magnitudeFilterEnabledChanged, this, &ProperMotionVectorsDialog::syncFromPlugin);
	connect(plugin, &ProperMotionVectors::properMotionFilterEnabledChanged, this, &ProperMotionVectorsDialog::syncFromPlugin);
	connect(plugin, &ProperMotionVectors::unknownRadialVelocityVisibleChanged, this, &ProperMotionVectorsDialog::syncFromPlugin);
	connect(plugin, &ProperMotionVectors::limitMagnitudeChanged, this, &ProperMotionVectorsDialog::syncFromPlugin);
	connect(plugin, &ProperMotionVectors::minimumProperMotionChanged, this, &ProperMotionVectorsDialog::syncFromPlugin);
	connect(plugin, &ProperMotionVectors::vectorLengthScaleChanged, this, &ProperMotionVectorsDialog::syncFromPlugin);
	connect(plugin, &ProperMotionVectors::radialVelocitySaturationChanged, this, &ProperMotionVectorsDialog::syncFromPlugin);

	syncFromPlugin();
	retranslate();
}

void ProperMotionVectorsDialog::syncFromPlugin()
{
	if (!dialog)
		return;

	enabledCheckBox->blockSignals(true);
	magnitudeFilterCheckBox->blockSignals(true);
	properMotionFilterCheckBox->blockSignals(true);
	unknownRadialVelocityCheckBox->blockSignals(true);
	limitMagnitudeSpinBox->blockSignals(true);
	minimumProperMotionSpinBox->blockSignals(true);
	vectorLengthScaleSpinBox->blockSignals(true);
	radialVelocitySaturationSpinBox->blockSignals(true);

	enabledCheckBox->setChecked(plugin->isEnabled());
	magnitudeFilterCheckBox->setChecked(plugin->isMagnitudeFilterEnabled());
	properMotionFilterCheckBox->setChecked(plugin->isProperMotionFilterEnabled());
	unknownRadialVelocityCheckBox->setChecked(plugin->isUnknownRadialVelocityVisible());
	limitMagnitudeSpinBox->setValue(plugin->getLimitMagnitude());
	minimumProperMotionSpinBox->setValue(plugin->getMinimumProperMotion());
	vectorLengthScaleSpinBox->setValue(plugin->getVectorLengthScale());
	radialVelocitySaturationSpinBox->setValue(plugin->getRadialVelocitySaturation());
	limitMagnitudeSpinBox->setEnabled(plugin->isMagnitudeFilterEnabled());
	minimumProperMotionSpinBox->setEnabled(plugin->isProperMotionFilterEnabled());

	enabledCheckBox->blockSignals(false);
	magnitudeFilterCheckBox->blockSignals(false);
	properMotionFilterCheckBox->blockSignals(false);
	unknownRadialVelocityCheckBox->blockSignals(false);
	limitMagnitudeSpinBox->blockSignals(false);
	minimumProperMotionSpinBox->blockSignals(false);
	vectorLengthScaleSpinBox->blockSignals(false);
	radialVelocitySaturationSpinBox->blockSignals(false);
}
