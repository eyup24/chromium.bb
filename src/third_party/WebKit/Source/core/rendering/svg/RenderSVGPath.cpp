/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2005, 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2009 Jeff Schiller <codedread@gmail.com>
 * Copyright (C) 2011 Renata Hodovan <reni@webkit.org>
 * Copyright (C) 2011 University of Szeged
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "core/rendering/svg/RenderSVGPath.h"

#include "core/rendering/svg/RenderSVGResourceMarker.h"
#include "core/rendering/svg/SVGResources.h"
#include "core/rendering/svg/SVGResourcesCache.h"
#include "core/rendering/svg/SVGSubpathData.h"
#include "core/svg/SVGGraphicsElement.h"

namespace blink {

RenderSVGPath::RenderSVGPath(SVGGraphicsElement* node)
    : RenderSVGShape(node)
{
}

RenderSVGPath::~RenderSVGPath()
{
}

void RenderSVGPath::updateShapeFromElement()
{
    RenderSVGShape::updateShapeFromElement();
    updateZeroLengthSubpaths();

    m_strokeBoundingBox = calculateUpdatedStrokeBoundingBox();
}

FloatRect RenderSVGPath::calculateUpdatedStrokeBoundingBox() const
{
    FloatRect strokeBoundingBox = m_strokeBoundingBox;

    if (!m_markerPositions.isEmpty())
        strokeBoundingBox.unite(markerRect(strokeWidth()));

    if (style()->svgStyle().hasStroke()) {
        // FIXME: zero-length subpaths do not respect vector-effect = non-scaling-stroke.
        float strokeWidth = this->strokeWidth();
        for (size_t i = 0; i < m_zeroLengthLinecapLocations.size(); ++i)
            strokeBoundingBox.unite(zeroLengthSubpathRect(m_zeroLengthLinecapLocations[i], strokeWidth));
    }

    return strokeBoundingBox;
}

bool RenderSVGPath::shapeDependentStrokeContains(const FloatPoint& point)
{
    if (RenderSVGShape::shapeDependentStrokeContains(point))
        return true;

    const SVGRenderStyle& svgStyle = style()->svgStyle();
    for (size_t i = 0; i < m_zeroLengthLinecapLocations.size(); ++i) {
        ASSERT(svgStyle.hasStroke());
        float strokeWidth = this->strokeWidth();
        if (svgStyle.capStyle() == SquareCap) {
            if (zeroLengthSubpathRect(m_zeroLengthLinecapLocations[i], strokeWidth).contains(point))
                return true;
        } else {
            ASSERT(svgStyle.capStyle() == RoundCap);
            FloatPoint radiusVector(point.x() - m_zeroLengthLinecapLocations[i].x(), point.y() -  m_zeroLengthLinecapLocations[i].y());
            if (radiusVector.lengthSquared() < strokeWidth * strokeWidth * .25f)
                return true;
        }
    }
    return false;
}

bool RenderSVGPath::shouldStrokeZeroLengthSubpath() const
{
    // Spec(11.4): Any zero length subpath shall not be stroked if the "stroke-linecap" property has a value of butt
    // but shall be stroked if the "stroke-linecap" property has a value of round or square
    return style()->svgStyle().hasStroke() && style()->svgStyle().capStyle() != ButtCap;
}

FloatRect RenderSVGPath::zeroLengthSubpathRect(const FloatPoint& linecapPosition, float strokeWidth)
{
    return FloatRect(linecapPosition.x() - strokeWidth / 2, linecapPosition.y() - strokeWidth / 2, strokeWidth, strokeWidth);
}

void RenderSVGPath::updateZeroLengthSubpaths()
{
    m_zeroLengthLinecapLocations.clear();

    if (!strokeWidth() || !shouldStrokeZeroLengthSubpath())
        return;

    SVGSubpathData subpathData(m_zeroLengthLinecapLocations);
    path().apply(&subpathData, SVGSubpathData::updateFromPathElement);
    subpathData.pathIsDone();
}

FloatRect RenderSVGPath::markerRect(float strokeWidth) const
{
    ASSERT(!m_markerPositions.isEmpty());

    SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(this);
    ASSERT(resources);

    RenderSVGResourceMarker* markerStart = resources->markerStart();
    RenderSVGResourceMarker* markerMid = resources->markerMid();
    RenderSVGResourceMarker* markerEnd = resources->markerEnd();
    ASSERT(markerStart || markerMid || markerEnd);

    FloatRect boundaries;
    unsigned size = m_markerPositions.size();
    for (unsigned i = 0; i < size; ++i) {
        if (RenderSVGResourceMarker* marker = SVGMarkerData::markerForType(m_markerPositions[i].type, markerStart, markerMid, markerEnd))
            boundaries.unite(marker->markerBoundaries(marker->markerTransformation(m_markerPositions[i].origin, m_markerPositions[i].angle, strokeWidth)));
    }
    return boundaries;
}

bool RenderSVGPath::shouldGenerateMarkerPositions() const
{
    if (!style()->svgStyle().hasMarkers())
        return false;

    if (!SVGResources::supportsMarkers(*toSVGGraphicsElement(element())))
        return false;

    SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(this);
    if (!resources)
        return false;

    return resources->markerStart() || resources->markerMid() || resources->markerEnd();
}

void RenderSVGPath::processMarkerPositions()
{
    m_markerPositions.clear();

    if (!shouldGenerateMarkerPositions())
        return;

    SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(this);
    ASSERT(resources);

    RenderSVGResourceMarker* markerStart = resources->markerStart();

    SVGMarkerData markerData(m_markerPositions, markerStart ? markerStart->orientType() == SVGMarkerOrientAutoStartReverse : false);
    path().apply(&markerData, SVGMarkerData::updateFromPathElement);
    markerData.pathIsDone();
}

}
