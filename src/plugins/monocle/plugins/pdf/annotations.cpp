/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2014  Georg Rudoy
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#include "annotations.h"
#include <QPolygonF>
#include <QtDebug>
#include <poppler-annotation.h>
#include <poppler-version.h>
#include "links.h"

namespace LeechCraft
{
namespace Monocle
{
namespace PDF
{
	IAnnotation_ptr MakeAnnotation (Document *doc, Poppler::Annotation *ann)
	{
		switch (ann->subType ())
		{
		case Poppler::Annotation::SubType::AText:
			return std::make_shared<TextAnnotation> (dynamic_cast<Poppler::TextAnnotation*> (ann));
		case Poppler::Annotation::SubType::AHighlight:
			return std::make_shared<HighlightAnnotation> (dynamic_cast<Poppler::HighlightAnnotation*> (ann));
#if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 20
		case Poppler::Annotation::SubType::ALink:
			if (ann->contents ().isEmpty ())
				return {};
			else
				return std::make_shared<LinkAnnotation> (doc, dynamic_cast<Poppler::LinkAnnotation*> (ann));
#endif
		case Poppler::Annotation::SubType::ACaret:
			return std::make_shared<CaretAnnotation> (dynamic_cast<Poppler::CaretAnnotation*> (ann));
		default:
			break;
		}

		qWarning () << Q_FUNC_INFO
				<< "unknown"
				<< ann->subType ();

		return {};
	}

	TextAnnotation::TextAnnotation (Poppler::TextAnnotation *ann)
	: AnnotationBase { ann }
	, TextAnn_ { ann }
	{
	}

	AnnotationType TextAnnotation::GetAnnotationType () const
	{
		return AnnotationType::Text;
	}

	bool TextAnnotation::IsInline () const
	{
		return TextAnn_->flags () & Poppler::TextAnnotation::InPlace;
	}

	HighlightAnnotation::HighlightAnnotation (Poppler::HighlightAnnotation *ann)
	: AnnotationBase { ann }
	, HighAnn_ { ann }
	{
	}

	AnnotationType HighlightAnnotation::GetAnnotationType () const
	{
		return AnnotationType::Highlight;
	}

	QList<QPolygonF> HighlightAnnotation::GetPolygons () const
	{
		QList<QPolygonF> result;
		for (const auto& quad : HighAnn_->highlightQuads ())
		{
			const auto pts = quad.points;
			result.append ({ { pts [0], pts [1], pts [2], pts [3] } });
		}
		return result;
	}

	LinkAnnotation::LinkAnnotation (Document *doc, Poppler::LinkAnnotation *ann)
	: AnnotationBase { ann }
	, LinkAnn_ { ann }
#if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 20
	, Link_ { new Link { doc, LinkAnn_->linkDestination (), {} } }
#endif
	{
	}

	AnnotationType LinkAnnotation::GetAnnotationType () const
	{
		return AnnotationType::Link;
	}

	ILink_ptr LinkAnnotation::GetLink () const
	{
		return Link_;
	}

	CaretAnnotation::CaretAnnotation (Poppler::CaretAnnotation *ann)
	: AnnotationBase { ann }
	{
	}

	AnnotationType CaretAnnotation::GetAnnotationType () const
	{
		return AnnotationType::Caret;
	}
}
}
}
