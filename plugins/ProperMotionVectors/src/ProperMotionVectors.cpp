/*
 * Proper Motion Vectors plug-in for Stellarium
 *
 * Author: Atque, assisted by Codex
 */

#include "ProperMotionVectors.hpp"

#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelModuleMgr.hpp"
#include "StelPainter.hpp"
#include "StelProjector.hpp"
#include "StelSphereGeometry.hpp"
#include "StarMgr.hpp"

#ifndef NO_GUI
#include "ProperMotionVectorsDialog.hpp"
#include "StelGui.hpp"
#include "StelGuiItems.hpp"
#endif

#include <QPainter>
#include <QPixmap>
#include <QPolygonF>
#include <QSettings>
#include <QtMath>

#ifndef NO_GUI
namespace
{
QPixmap makeProperMotionVectorsFallbackIcon(bool active)
{
	QPixmap pixmap(32, 32);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing, true);

	const QColor starColor = active ? QColor(216, 216, 216) : QColor(135, 135, 135);
	const QColor arrowColor = active ? QColor(240, 240, 240) : QColor(176, 176, 176);
	QPolygonF star;
	const QPointF center(11.5, 19.5);
	for (int i=0; i<10; ++i)
	{
		const double angle = -M_PI_2 + i*M_PI/5.0;
		const double radius = (i%2==0) ? 7.0 : 3.1;
		star << QPointF(center.x() + std::cos(angle)*radius, center.y() + std::sin(angle)*radius);
	}
	painter.setPen(QPen(QColor(20, 20, 20, 150), 1.2));
	painter.setBrush(starColor);
	painter.drawPolygon(star);
	painter.setPen(QPen(arrowColor, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
	painter.drawLine(QPointF(13.5, 17.5), QPointF(25.5, 6.5));
	painter.drawLine(QPointF(25.5, 6.5), QPointF(24.5, 14.0));
	painter.drawLine(QPointF(25.5, 6.5), QPointF(18.0, 7.5));
	return pixmap;
}

QPixmap makeToolbarIcon(QPixmap pixmap, bool active)
{
	if (pixmap.isNull())
		pixmap = makeProperMotionVectorsFallbackIcon(active);
	return pixmap.scaled(160, 160, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}
}
#endif

StelModule* ProperMotionVectorsStelPluginInterface::getStelModule() const
{
	return new ProperMotionVectors();
}

StelPluginInfo ProperMotionVectorsStelPluginInterface::getPluginInfo() const
{
	StelPluginInfo info;
	info.id = "ProperMotionVectors";
	info.displayedName = "Proper Motion Vectors";
	info.authors = "Atque (assisted by Codex)";
	info.contact = "www.stellarium.org";
	info.description = "Shows date-aware stellar proper motion vectors colored by radial velocity.";
	info.version = PROPERMOTIONVECTORS_PLUGIN_VERSION;
	info.license = PROPERMOTIONVECTORS_PLUGIN_LICENSE;
	return info;
}

ProperMotionVectors::ProperMotionVectors()
{
	setObjectName("ProperMotionVectors");
#ifndef NO_GUI
	configDialog = new ProperMotionVectorsDialog(this);
#endif
}

ProperMotionVectors::~ProperMotionVectors()
{
#ifndef NO_GUI
	delete configDialog;
#endif
}

void ProperMotionVectors::init()
{
	conf = StelApp::getInstance().getSettings();
	starMgr = GETSTELMODULE(StarMgr);

	loadSettings();

	addAction("actionShow_ProperMotionVectors", N_("Proper Motion Vectors"), N_("Show proper motion vectors"), "enabled");
#ifndef NO_GUI
	addAction("actionShow_ProperMotionVectors_dialog", N_("Proper Motion Vectors"), N_("Proper motion vector settings"), configDialog, "visible");
	ensureToolbarButton();
#endif
}

void ProperMotionVectors::update(double deltaTime)
{
	Q_UNUSED(deltaTime);
#ifndef NO_GUI
	ensureToolbarButton();
#endif
}

#ifndef NO_GUI
void ProperMotionVectors::ensureToolbarButton()
{
	if (toolbarButton!=nullptr)
		return;

	Q_INIT_RESOURCE(ProperMotionVectors);
	StelGui* gui = dynamic_cast<StelGui*>(StelApp::getInstance().getGui());
	if (gui!=nullptr)
	{
		QPixmap iconOn(":/ProperMotionVectors/bt_ProperMotionVectors_On.xpm");
		QPixmap iconOff(":/ProperMotionVectors/bt_ProperMotionVectors_Off.xpm");
		if (iconOn.isNull() || iconOff.isNull())
			qWarning() << "Unable to load Proper Motion Vectors toolbar icons";
		iconOn = makeToolbarIcon(iconOn, true);
		iconOff = makeToolbarIcon(iconOff, false);
		toolbarButton = new StelButton(nullptr,
					       iconOn,
					       iconOff,
					       QPixmap(":/graphicGui/miscGlow32x32.png"),
					       QString(),
					       false,
					       "actionShow_ProperMotionVectors_dialog");
		toolbarButton->setChecked(enabled);
		connect(toolbarButton, &StelButton::triggered, this, [this] { setEnabled(!enabled); });
		connect(this, &ProperMotionVectors::enabledChanged, toolbarButton, QOverload<bool>::of(&StelButton::setChecked));
		gui->getButtonBar()->addButton(toolbarButton, "065-pluginsGroup", "actionShow_Angle_Measure");
	}
}
#endif

void ProperMotionVectors::deinit()
{
	starMgr = nullptr;
}

double ProperMotionVectors::getCallOrder(StelModuleActionName actionName) const
{
	if (actionName==StelModule::ActionDraw)
		return StelApp::getInstance().getModuleMgr().getModule("StarMgr")->getCallOrder(actionName)+10.;
	return 0.;
}

bool ProperMotionVectors::configureGui(bool show)
{
#ifdef NO_GUI
	Q_UNUSED(show)
	return false;
#else
	if (show)
		configDialog->setVisible(true);
	return true;
#endif
}

void ProperMotionVectors::draw(StelCore* core)
{
	if (!enabled || starMgr==nullptr || !core->getFlagClearSky())
		return;

	const StelProjectorP projector = core->getProjection(StelCore::FrameJ2000);
	const SphericalRegionP viewport = projector->getViewportConvexPolygon(10.f, 10.f);
	if (viewport.isNull())
		return;
	SphericalRegionP collectionRegion = projector->getViewportConvexPolygon(64.f, 64.f);
	if (collectionRegion.isNull())
		collectionRegion = viewport;

	StelPainter painter(projector);
	painter.setBlending(true);

	const QVector<StarKinematics>& vectors = updateVectorCache(core, collectionRegion);
	QVector<Vec2f> vertices;
	QVector<Vec3f> colors;
	vertices.reserve(vectors.size()*6);
	colors.reserve(vectors.size()*6);
	for (const StarKinematics& kinematics : vectors)
		appendStarVector(vertices, colors, viewport, projector, kinematics);

	if (vertices.isEmpty())
		return;

	painter.enableClientStates(true, false, true);
	painter.setVertexPointer(2, GL_FLOAT, vertices.constData());
	painter.setColorPointer(3, GL_FLOAT, colors.constData());
	painter.drawFromArray(StelPainter::Lines, vertices.size(), 0, false);
	painter.enableClientStates(false);
}

const QVector<StarKinematics>& ProperMotionVectors::updateVectorCache(StelCore* core, const SphericalRegionP& collectionRegion)
{
	const double jde = core->getJDE();
	const double minPM = minimumProperMotion*1000.0;
	const bool useParallax = core->getUseParallax();
	const bool useAberration = core->getUseAberration();
	const double parallaxFactor = core->getParallaxFactor();
	const bool needsRefresh = cachedRegion.isNull()
		|| !cachedRegion->contains(collectionRegion)
		|| std::fabs(jde-cachedJDE) > 1.0
		|| !qFuzzyCompare(limitMagnitude, cachedLimitMagnitude)
		|| !qFuzzyCompare(minPM+1.0, cachedMinimumProperMotion+1.0)
		|| magnitudeFilterEnabled != cachedMagnitudeFilterEnabled
		|| properMotionFilterEnabled != cachedProperMotionFilterEnabled
		|| unknownRadialVelocityVisible != cachedUnknownRadialVelocityVisible
		|| useParallax != cachedUseParallax
		|| useAberration != cachedUseAberration
		|| !qFuzzyCompare(parallaxFactor+1.0, cachedParallaxFactor+1.0);

	if (needsRefresh)
	{
		StarKinematicsQuery query;
		query.magnitudeLimit = static_cast<float>(limitMagnitude);
		query.properMotionLimit = static_cast<float>(minPM);
		query.useMagnitudeLimit = magnitudeFilterEnabled;
		query.useProperMotionLimit = properMotionFilterEnabled;
		query.requireRadialVelocity = !unknownRadialVelocityVisible;
		cachedVectors = starMgr->collectStarKinematics(collectionRegion, core, query);
		cachedRegion = collectionRegion;
		cachedJDE = jde;
		cachedLimitMagnitude = limitMagnitude;
		cachedMinimumProperMotion = minPM;
		cachedMagnitudeFilterEnabled = magnitudeFilterEnabled;
		cachedProperMotionFilterEnabled = properMotionFilterEnabled;
		cachedUnknownRadialVelocityVisible = unknownRadialVelocityVisible;
		cachedUseParallax = useParallax;
		cachedUseAberration = useAberration;
		cachedParallaxFactor = parallaxFactor;
	}

	return cachedVectors;
}

void ProperMotionVectors::appendStarVector(QVector<Vec2f>& vertices, QVector<Vec3f>& colors, const SphericalRegionP& viewport, const StelProjectorP& projector, const StarKinematics& kinematics) const
{
	if (kinematics.totalProperMotion <= 0.0)
		return;
	if (!viewport->contains(kinematics.position))
		return;

	Vec3d start;
	if (!projector->projectCheck(kinematics.position, start))
		return;

	Vec3d tangent = kinematics.properMotion - kinematics.position*(kinematics.properMotion*kinematics.position);
	if (tangent.normSquared() <= 0.0)
		return;
	tangent.normalize();

	Vec3d samplePos = kinematics.position + tangent * (0.01*M_PI/180.0);
	samplePos.normalize();
	Vec3d sample;
	if (!projector->projectCheck(samplePos, sample))
		return;

	Vec2f direction(static_cast<float>(sample[0]-start[0]), static_cast<float>(sample[1]-start[1]));
	const float directionLength = std::sqrt(direction[0]*direction[0] + direction[1]*direction[1]);
	if (directionLength <= 0.f)
		return;
	direction /= directionLength;

	const double pmArcsec = kinematics.totalProperMotion / 1000.0;
	const float length = static_cast<float>(pmArcsec * vectorLengthScale);
	if (length < 1.f)
		return;

	const float x1 = static_cast<float>(start[0]);
	const float y1 = static_cast<float>(start[1]);
	const float x2 = x1 + direction[0]*length;
	const float y2 = y1 + direction[1]*length;

	const Vec3f color = kinematics.hasRadialVelocity ? colorForRadialVelocity(kinematics.radialVelocity, radialVelocitySaturation) : Vec3f(1.f, 0.85f, 0.12f);
	vertices << Vec2f(x1, y1) << Vec2f(x2, y2);
	colors << color << color;

	const float arrowLength = qBound(5.f, length*0.28f, 12.f);
	const float arrowWidth = arrowLength*0.55f;
	const Vec2f normal(-direction[1], direction[0]);
	vertices << Vec2f(x2, y2)
		 << Vec2f(x2 - direction[0]*arrowLength + normal[0]*arrowWidth, y2 - direction[1]*arrowLength + normal[1]*arrowWidth)
		 << Vec2f(x2, y2)
		 << Vec2f(x2 - direction[0]*arrowLength - normal[0]*arrowWidth, y2 - direction[1]*arrowLength - normal[1]*arrowWidth);
	colors << color << color << color << color;
}

Vec3f ProperMotionVectors::colorForRadialVelocity(double radialVelocity, double saturation)
{
	const double scale = qMax(1.0, saturation);
	const float t = static_cast<float>(qBound(0.0, std::tanh(std::fabs(radialVelocity)/(scale*2.0)), 0.92));
	if (radialVelocity < 0.0)
		return Vec3f(1.f-t, 1.f-t, 1.f);
	if (radialVelocity > 0.0)
		return Vec3f(1.f, 1.f-t, 1.f-t);
	return Vec3f(1.f, 1.f, 1.f);
}

void ProperMotionVectors::setEnabled(bool value)
{
	if (enabled == value)
		return;
	enabled = value;
	saveSettings();
	emit enabledChanged(value);
}

void ProperMotionVectors::setMagnitudeFilterEnabled(bool value)
{
	if (magnitudeFilterEnabled == value)
		return;
	magnitudeFilterEnabled = value;
	saveSettings();
	emit magnitudeFilterEnabledChanged(value);
}

void ProperMotionVectors::setProperMotionFilterEnabled(bool value)
{
	if (properMotionFilterEnabled == value)
		return;
	properMotionFilterEnabled = value;
	saveSettings();
	emit properMotionFilterEnabledChanged(value);
}

void ProperMotionVectors::setUnknownRadialVelocityVisible(bool value)
{
	if (unknownRadialVelocityVisible == value)
		return;
	unknownRadialVelocityVisible = value;
	saveSettings();
	emit unknownRadialVelocityVisibleChanged(value);
}

void ProperMotionVectors::setLimitMagnitude(double value)
{
	value = qBound(-2.0, value, 25.0);
	if (qFuzzyCompare(limitMagnitude, value))
		return;
	limitMagnitude = value;
	saveSettings();
	emit limitMagnitudeChanged(value);
}

void ProperMotionVectors::setMinimumProperMotion(double value)
{
	value = qMax(0.0, value);
	if (qFuzzyCompare(minimumProperMotion, value))
		return;
	minimumProperMotion = value;
	saveSettings();
	emit minimumProperMotionChanged(value);
}

void ProperMotionVectors::setVectorLengthScale(double value)
{
	value = qBound(1.0, value, 1000.0);
	if (qFuzzyCompare(vectorLengthScale, value))
		return;
	vectorLengthScale = value;
	saveSettings();
	emit vectorLengthScaleChanged(value);
}

void ProperMotionVectors::setRadialVelocitySaturation(double value)
{
	value = qBound(1.0, value, 3000.0);
	if (qFuzzyCompare(radialVelocitySaturation, value))
		return;
	radialVelocitySaturation = value;
	saveSettings();
	emit radialVelocitySaturationChanged(value);
}

void ProperMotionVectors::restoreDefaults()
{
	enabled = false;
	magnitudeFilterEnabled = true;
	properMotionFilterEnabled = true;
	unknownRadialVelocityVisible = true;
	limitMagnitude = 6.0;
	minimumProperMotion = 1.0;
	vectorLengthScale = 40.0;
	radialVelocitySaturation = 600.0;
	saveSettings();
	emit enabledChanged(enabled);
	emit magnitudeFilterEnabledChanged(magnitudeFilterEnabled);
	emit properMotionFilterEnabledChanged(properMotionFilterEnabled);
	emit unknownRadialVelocityVisibleChanged(unknownRadialVelocityVisible);
	emit limitMagnitudeChanged(limitMagnitude);
	emit minimumProperMotionChanged(minimumProperMotion);
	emit vectorLengthScaleChanged(vectorLengthScale);
	emit radialVelocitySaturationChanged(radialVelocitySaturation);
}

void ProperMotionVectors::loadSettings()
{
	Q_ASSERT(conf);
	enabled = conf->value("ProperMotionVectors/enabled", false).toBool();
	magnitudeFilterEnabled = conf->value("ProperMotionVectors/magnitude_filter_enabled", true).toBool();
	properMotionFilterEnabled = conf->value("ProperMotionVectors/proper_motion_filter_enabled", true).toBool();
	unknownRadialVelocityVisible = conf->value("ProperMotionVectors/unknown_radial_velocity_visible", true).toBool();
	limitMagnitude = conf->value("ProperMotionVectors/limit_magnitude", 6.0).toDouble();
	minimumProperMotion = conf->value("ProperMotionVectors/minimum_proper_motion", 1.0).toDouble();
	vectorLengthScale = conf->value("ProperMotionVectors/vector_length_scale", 40.0).toDouble();
	radialVelocitySaturation = conf->value("ProperMotionVectors/radial_velocity_saturation", 600.0).toDouble();
}

void ProperMotionVectors::saveSettings() const
{
	if (conf==nullptr)
		return;
	conf->setValue("ProperMotionVectors/enabled", enabled);
	conf->setValue("ProperMotionVectors/magnitude_filter_enabled", magnitudeFilterEnabled);
	conf->setValue("ProperMotionVectors/proper_motion_filter_enabled", properMotionFilterEnabled);
	conf->setValue("ProperMotionVectors/unknown_radial_velocity_visible", unknownRadialVelocityVisible);
	conf->setValue("ProperMotionVectors/limit_magnitude", limitMagnitude);
	conf->setValue("ProperMotionVectors/minimum_proper_motion", minimumProperMotion);
	conf->setValue("ProperMotionVectors/vector_length_scale", vectorLengthScale);
	conf->setValue("ProperMotionVectors/radial_velocity_saturation", radialVelocitySaturation);
}
