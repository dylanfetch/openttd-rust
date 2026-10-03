/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file widget-parser-comparison.cpp Scoped semantic/ownership parser comparison. */
#include "stdafx.h"
#include "window_gui.h"
#include "tests/mock_environment.h"
#include <iostream>
#include <sstream>

extern std::unique_ptr<NWidgetBase> OriginalMakeNWidgets(std::span<const NWidgetPart>, std::unique_ptr<NWidgetBase> &&);
extern std::unique_ptr<NWidgetBase> OriginalMakeWindowNWidgetTree(std::span<const NWidgetPart>, NWidgetStacked **);
extern std::vector<WindowDesc *> *_window_descs;

static bool candidate = false;
static unsigned live = 0;
static NWidgetStacked **observed_shade = nullptr;
static std::vector<std::string> trace;

static void Require(bool value) { if (!value) throw std::runtime_error("fixture requirement"); }

/* Legal protected-member access, without casting a base object to this helper. */
struct ContainerChildren : NWidgetContainer {
	static constexpr auto member = &ContainerChildren::children;
};

static std::string Tree(NWidgetBase *widget, const NWidgetStacked *shade = nullptr)
{
	if (widget == nullptr) return "null";
	std::ostringstream result;
	result << int(widget->type) << ':' << widget->GetIndex() << ':' << widget->fill_x << ',' << widget->fill_y
		<< ':' << widget->resize_x << ',' << widget->resize_y << ":padding=" << int(widget->uz_padding.top)
		<< ',' << int(widget->uz_padding.right) << ',' << int(widget->uz_padding.bottom) << ',' << int(widget->uz_padding.left);
	if (auto resized = dynamic_cast<NWidgetResizeBase *>(widget)) result << ":min=" << resized->uz_min_x << ',' << resized->uz_min_y;
	if (widget == shade) result << ":shade";
	if (auto container = dynamic_cast<NWidgetContainer *>(widget)) {
		result << '[';
		for (const auto &child : container->*ContainerChildren::member) result << Tree(child.get(), shade) << ';';
		result << ']';
	} else if (auto background = dynamic_cast<NWidgetBackground *>(widget)) {
		/* These bounded shapes use the unchanged default background child, which
		 * is the first vertical widget returned by this existing public query. */
		result << '{' << Tree(background->GetWidgetOfType(NWID_VERTICAL), shade) << '}';
	}
	return result.str();
}

struct TrackedWidget : NWidgetVertical {
	explicit TrackedWidget(int id) : NWidgetVertical({}, id)
	{
		++live;
		trace.push_back("create:" + std::to_string(id));
	}
	~TrackedWidget() override
	{
		--live;
		trace.push_back("destroy:" + std::to_string(this->GetIndex()));
	}
};

static std::unique_ptr<NWidgetBase> FirstTracked() { trace.push_back("callback:1"); return std::make_unique<TrackedWidget>(71); }
static std::unique_ptr<NWidgetBase> SecondTracked() { trace.push_back("callback:2"); return std::make_unique<TrackedWidget>(72); }
static std::unique_ptr<NWidgetBase> Throwing()
{
	trace.push_back("callback:throw");
	if (observed_shade != nullptr) trace.push_back(*observed_shade != nullptr ? "shade:written" : "shade:clear");
	throw std::runtime_error("injected widget generator");
}
static std::unique_ptr<NWidgetBase> NullWidget() { std::cout << "null-callback\n" << std::flush; return nullptr; }

static std::unique_ptr<NWidgetBase> Reentrant()
{
	trace.push_back("callback:reentrant");
	static constexpr NWidgetPart parts[] = {
		NWidget(NWID_SPACER), SetMinimalSize(7, 9), SetPadding(2),
	};
	std::unique_ptr<NWidgetBase> root = std::make_unique<NWidgetHorizontal>(NWidContainerFlags{}, 80);
	return candidate ? MakeNWidgets(parts, std::move(root)) : OriginalMakeNWidgets(parts, std::move(root));
}

static std::unique_ptr<NWidgetBase> CompleteSubtree()
{
	trace.push_back("callback:complete");
	auto root = std::make_unique<NWidgetHorizontal>(NWidContainerFlags{}, 90);
	root->Add(std::make_unique<NWidgetSpacer>(11, 13));
	return root;
}

static std::string Events()
{
	std::string result;
	for (const auto &event : trace) result += ':' + event;
	return result;
}

static void Compare(std::string_view name, std::span<const NWidgetPart> parts, bool window = false, bool partial = false)
{
	std::string records[2];
	for (unsigned version = 0; version < 2; ++version) {
		candidate = version != 0;
		Require(live == 0);
		trace.clear();
		std::unique_ptr<NWidgetBase> owner;
		if (partial) {
			owner = std::make_unique<NWidgetVertical>(NWidContainerFlags{}, 100);
			static_cast<NWidgetContainer *>(owner.get())->Add(std::make_unique<TrackedWidget>(70));
		}
		/* The entry clear must replace a preexisting nonnull sentinel. Never dereference it. */
		NWidgetStacked *shade = reinterpret_cast<NWidgetStacked *>(uintptr_t{1});
		observed_shade = window ? &shade : nullptr;
		std::string outcome;
		try {
			if (window) {
				owner = candidate ? MakeWindowNWidgetTree(parts, &shade) : OriginalMakeWindowNWidgetTree(parts, &shade);
			} else {
				owner = candidate ? MakeNWidgets(parts, std::move(owner)) : OriginalMakeNWidgets(parts, std::move(owner));
			}
			outcome = "ok";
		} catch (const std::runtime_error &error) {
			outcome = "runtime_error:" + std::string(error.what());
		}
		/* A failed window may leave a dangling shade output; only callbacks inspect
		 * it while the shade object is live. No failure-side pointer dereference. */
		const bool success = outcome == "ok";
		if (name == "trailing-end") {
#ifdef WITH_ASSERT
			Require(outcome == "runtime_error:Did not consume all NWidgetParts");
#else
			Require(success);
#endif
		} else if (name == "partial-owner-attribute-throw") {
			Require(outcome == "runtime_error:WPT_TEXTSTYLE requires NWidgetCore");
		} else if (name.ends_with("throw")) {
			Require(outcome == "runtime_error:injected widget generator");
		} else {
			Require(success);
		}
		records[version] = outcome + ":tree=" + Tree(owner.get(), window && success ? shade : nullptr)
			+ ":live=" + std::to_string(live) + Events();
		if (window && success) {
			if (shade != nullptr) Require(Tree(owner.get(), shade).find(":shade") != std::string::npos);
			records[version] += shade != nullptr ? ":shade:inserted" : ":shade:none";
		}
		if (partial && !success) {
			Require(owner != nullptr);
			Require(live == 2); // Preexisting and already committed, never the failed child.
		}
		owner.reset();
		Require(live == 0);
		records[version] += ":cleanup" + Events();
	}
	if (records[0] != records[1]) {
		std::cerr << name << "\noriginal " << records[0] << "\ncandidate " << records[1] << '\n';
		std::exit(1);
	}
	std::cout << name << ' ' << records[0] << '\n';
}

int main(int argc, char **argv)
{
	MockEnvironment::Instance();
	if (argc == 3 && std::string_view(argv[1]) == "--null") {
		candidate = std::string_view(argv[2]) == "candidate";
		static constexpr NWidgetPart parts[] = { NWidget(NWID_SPACER), NWidgetFunction(NullWidget), NWidget(NWID_SPACER) };
		std::unique_ptr<NWidgetBase> owner = std::make_unique<NWidgetVertical>();
		auto root = candidate ? MakeNWidgets(parts, std::move(owner)) : OriginalMakeNWidgets(parts, std::move(owner));
		std::cout << "null-result " << Tree(root.get()) << '\n';
		return 0;
	}
	std::cout << "registered " << _window_descs->size() << '\n';
	static constexpr NWidgetPart nested[] = {
		NWidget(NWID_HORIZONTAL, NWidContainerFlags{}, 1), SetPadding(1, 2, 3, 4),
			NWidget(WWT_PANEL, COLOUR_BLUE, 2), SetMinimalSize(20, 30), SetFill(1, 2),
				NWidget(NWID_VERTICAL, NWidContainerFlags{}, 3),
					NWidget(NWID_SPACER), SetMinimalSize(6, 7), SetResize(4, 5),
				EndContainer(),
			EndContainer(),
			NWidget(NWID_SPACER), SetPadding(8),
		EndContainer(),
	};
	Compare("nested-background-attributes", nested);
	static constexpr NWidgetPart title[] = {
		NWidget(NWID_HORIZONTAL_LTR, NWidContainerFlags{}, 10), NWidget(WWT_CAPTION, COLOUR_BLUE, 11),
		NWidget(WWT_SHADEBOX, COLOUR_BLUE, 12), EndContainer(),
		NWidget(WWT_PANEL, COLOUR_RED, 13), NWidget(NWID_SPACER), EndContainer(),
	};
	Compare("shade-body", title, true);
	Compare("shade-no-body", std::span(title).first(4), true);
	static constexpr NWidgetPart no_shade[] = {
		NWidget(NWID_HORIZONTAL, NWidContainerFlags{}, 20), NWidget(WWT_CAPTION, COLOUR_BLUE, 21), EndContainer(), NWidget(NWID_SPACER),
	};
	Compare("caption-without-shade", no_shade, true);
	static constexpr NWidgetPart function[] = {
		NWidgetFunction(CompleteSubtree), SetPadding(3), NWidget(NWID_SPACER), SetMinimalSize(17, 19),
	};
	Compare("complete-function-followed", function);
	static constexpr NWidgetPart reentrant[] = { NWidgetFunction(Reentrant), NWidget(NWID_SPACER), SetPadding(4) };
	Compare("reentrant", reentrant);
	static constexpr NWidgetPart eof[] = { NWidget(NWID_VERTICAL, NWidContainerFlags{}, 30), NWidget(NWID_SPACER), SetMinimalSize(1, 2) };
	Compare("permissive-eof", eof);
	static constexpr NWidgetPart trailing[] = { NWidget(NWID_SPACER), EndContainer(), NWidget(NWID_SPACER) };
	Compare("trailing-end", trailing);
	static constexpr NWidgetPart attribute_throw[] = {
		NWidgetFunction(FirstTracked), NWidgetFunction(SecondTracked), SetTextStyle(TC_RED),
	};
	Compare("partial-owner-attribute-throw", attribute_throw, false, true);
	static constexpr NWidgetPart generator_throw[] = { NWidgetFunction(FirstTracked), NWidgetFunction(Throwing) };
	Compare("partial-owner-generator-throw", generator_throw, false, true);
	static constexpr NWidgetPart shade_throw[] = {
		NWidget(NWID_HORIZONTAL, NWidContainerFlags{}, 40), NWidget(WWT_CAPTION, COLOUR_BLUE, 41),
		NWidget(WWT_SHADEBOX, COLOUR_BLUE, 42), EndContainer(), NWidgetFunction(Throwing),
	};
	Compare("shade-before-body-throw", shade_throw, true);
	static constexpr NWidgetPart entry_throw[] = { NWidgetFunction(Throwing) };
	Compare("shade-clear-before-first-throw", entry_throw, true);
}
