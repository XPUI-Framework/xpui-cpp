//! What the host says about itself.
//!
//! The screen that exists to make the non-drawing half of the boundary
//! visible: every figure on it arrives through a `xpui_host_*` call, so a
//! symbol that was declared but never defined shows up here as a blank or a
//! zero rather than as a link error nobody reads.
//!
//! The figures are formatted **when they are read**, not in `body()`.
//! `body()` runs on every paint and on every frame carrying input; formatting
//! six numbers there would allocate six strings several times a second and
//! drag `core::fmt` onto a path that has to stay cheap.

use alloc::format;
use alloc::string::String;

use xpui::screen::Screen;
use xpui::{Hint, List, ListRow, NavigationScreen, ScrollView, Section, View, vstack};

use crate::strings::tr;
use crate::{device, heap};

/// Everything this screen can be told.
#[derive(Copy, Clone)]
pub enum Msg {
    /// Take the readings again. E-ink costs a second a repaint, so this is a
    /// button rather than something that happens every frame.
    Refresh,
}

/// The battery and the five memory figures, already written out.
struct Figures {
    battery: String,
    total: String,
    used: String,
    free: String,
    largest_block: String,
    min_free: String,
}

impl Figures {
    fn read() -> Self {
        let heap = heap::reading();
        let battery = device::battery_percent();

        Figures {
            battery: if battery < 0 {
                String::from(UNKNOWN)
            } else {
                format!("{battery}%")
            },
            total: kilobytes(heap.total),
            used: kilobytes(heap.used()),
            free: kilobytes(heap.free),
            largest_block: kilobytes(heap.largest_block),
            min_free: kilobytes(heap.min_free),
        }
    }
}

/// What a figure the host cannot measure looks like.
///
/// Every one of these is a number the host answered **negatively** to, which
/// is the ABI's way of saying "I cannot tell you" — distinct from zero, and
/// showing it as `0 KB` would be a made-up answer rather than a missing one.
///
/// ASCII on purpose: the SDK's bundled face is Latin-1, and an em dash renders
/// as an empty box.
const UNKNOWN: &str = "n/a";

/// Bytes as kilobytes, rounded rather than truncated so a figure just under a
/// kilobyte does not read as nothing at all.
///
/// Saturating, because the rounding term is added to a number that came across
/// the FFI: `i32::MAX` would otherwise overflow here rather than at the seam
/// it arrived through.
fn kilobytes(bytes: i32) -> String {
    if bytes < 0 {
        return String::from(UNKNOWN);
    }
    format!("{} KB", bytes.saturating_add(512) / 1024)
}

/// The device and its heap, as they were when last read.
pub struct About {
    figures: Figures,
}

impl Default for About {
    fn default() -> Self {
        About::new()
    }
}

impl About {
    /// Takes the first reading. `Msg::Refresh` takes another; on e-ink a
    /// repaint costs a second, so it is a row rather than a frame timer.
    pub fn new() -> Self {
        About {
            figures: Figures::read(),
        }
    }

    fn device_rows(&self) -> List<Msg> {
        List::new()
            .push(ListRow::new(tr(c"STR_ABOUT_DEVICE")).value(device::name()))
            .push(ListRow::new(tr(c"STR_ABOUT_VERSION")).value(device::firmware_version()))
            .push(ListRow::new(tr(c"STR_ABOUT_BATTERY")).value(&self.figures.battery))
    }

    fn memory_rows(&self) -> List<Msg> {
        List::new()
            // Total first, then what is gone: a free figure on its own says
            // nothing about how much room there ever was.
            .push(ListRow::new(tr(c"STR_ABOUT_TOTAL")).value(&self.figures.total))
            .push(ListRow::new(tr(c"STR_ABOUT_USED")).value(&self.figures.used))
            .push(ListRow::new(tr(c"STR_ABOUT_FREE")).value(&self.figures.free))
            .push(ListRow::new(tr(c"STR_ABOUT_LARGEST")).value(&self.figures.largest_block))
            .push(ListRow::new(tr(c"STR_ABOUT_MIN_FREE")).value(&self.figures.min_free))
            .push(ListRow::new(tr(c"STR_ABOUT_REFRESH")).on_tap(Msg::Refresh))
    }
}

impl Screen for About {
    type Message = Msg;

    fn body(&self) -> impl View<Self::Message> {
        NavigationScreen::new(ScrollView::new(vstack![12;
            Section::new(tr(c"STR_ABOUT_HOST"), self.device_rows()),
            Section::new(tr(c"STR_ABOUT_MEMORY"), self.memory_rows()),
        ]))
        .title(tr(c"STR_ABOUT_TITLE"))
        .hints(
            Hint::Standard,
            Hint::text(tr(c"STR_ABOUT_REFRESH")),
            Hint::Standard,
            Hint::Standard,
        )
    }

    fn update(&mut self, message: Self::Message) {
        match message {
            Msg::Refresh => self.figures = Figures::read(),
        }
    }

    fn title(&self) -> Option<&'static str> {
        Some(tr(c"STR_ABOUT_TITLE"))
    }
}
