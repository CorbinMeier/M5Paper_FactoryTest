#pragma once
// m5paper-ui -- umbrella header.
//
// Convenient for an app that uses most of the library. Note the tradeoff: a
// module included here is compiled in whether or not it is used, because
// PlatformIO's Library Dependency Finder resolves by #include (issue #86). An
// app that wants the smallest possible image should include only the specific
// headers it needs and skip this file.
//
//   #include <m5paper_ui.h>
//
//   class HomeScreen : public m5ui::Screen { ... };
//
//   void setup() {
//       auto& dev = m5ui::Device::Get();
//       dev.Begin();
//       static MyApp app;
//       dev.Run(app);
//   }

// -------------------------------------------------------------- core -------
#include "core/app.h"
#include "core/calendar_math.h"
#include "core/device.h"
#include "core/display.h"
#include "core/focus.h"
#include "core/geometry.h"
#include "core/input.h"
#include "core/layout.h"
#include "core/memory.h"
#include "core/power.h"
#include "core/screen.h"
#include "core/specs.h"
#include "core/text.h"
#include "core/tokens.h"
#include "core/widget.h"

// -------------------------------------------------------- components -------
#include "components/button.h"
#include "components/container.h"
#include "components/label.h"
#include "components/listview.h"
#include "components/progressbar.h"
#include "components/scrollview.h"
#include "components/statusbar.h"
#include "components/textinput.h"

// ----------------------------------------------------------- modules -------
#include "modules/keyboard.h"
#include "modules/storage.h"

#define M5PAPER_UI_VERSION "0.1.0"
