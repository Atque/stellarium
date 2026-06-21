/*
 * Proper Motion Vectors plug-in settings dialog.
 *
 * Author: Atque, assisted by Codex
 */

#ifndef PROPERMOTIONVECTORSDIALOG_HPP
#define PROPERMOTIONVECTORSDIALOG_HPP

#include "StelDialog.hpp"

class ProperMotionVectors;
class QCheckBox;
class QDoubleSpinBox;
class QTabWidget;
class QTextBrowser;

class ProperMotionVectorsDialog : public StelDialog
{
	Q_OBJECT
public:
	explicit ProperMotionVectorsDialog(ProperMotionVectors* plugin);

public slots:
	void retranslate() override;

protected:
	void createDialogContent() override;

private slots:
	void syncFromPlugin();

private:
	ProperMotionVectors* plugin;
	class TitleBar* titleBar = nullptr;
	QTabWidget* tabWidget = nullptr;
	QTextBrowser* aboutTextBrowser = nullptr;
	QCheckBox* enabledCheckBox = nullptr;
	QCheckBox* magnitudeFilterCheckBox = nullptr;
	QCheckBox* properMotionFilterCheckBox = nullptr;
	QCheckBox* unknownRadialVelocityCheckBox = nullptr;
	QDoubleSpinBox* limitMagnitudeSpinBox = nullptr;
	QDoubleSpinBox* minimumProperMotionSpinBox = nullptr;
	QDoubleSpinBox* vectorLengthScaleSpinBox = nullptr;
	QDoubleSpinBox* radialVelocitySaturationSpinBox = nullptr;
};

#endif
