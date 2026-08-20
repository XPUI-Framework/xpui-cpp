//! The drawing the host does not do itself.
//!
//! A slider, a stepper, a toggle and a dialog. Every one of them is painted by
//! FreeInkUI rather than from primitives, so this screen is what proves the
//! `Chrome` half of the ABI arrives intact — and the dialog in particular is
//! the one that rots quietly, because a scrim is only ever exercised by an
//! overlay and a host without one links perfectly happily.

use alloc::format;
use alloc::string::String;

use xpui::screen::Screen;
use xpui::{
    Button, Divider, Hint, List, ListRow, Modal, NavigationScreen, ProgressBar, Scrim, ScrollView,
    Slider, Stepper, Toggle, View, vstack,
};

use crate::strings::tr;

/// Everything this screen can be told.
#[derive(Copy, Clone)]
pub enum Msg {
    /// The slider dragged or the track tapped: an absolute value.
    Level(i32),
    /// The stepper nudged: ±1.
    StepLevel(i32),
    /// The toggle moved, carrying the state it is moving *to*.
    Frontlight(bool),
    /// The refresh-mode row: opens the picker.
    PickMode,
    /// A mode chosen from the picker.
    ChoseMode(usize),
    /// The picker dismissed without choosing.
    DismissPicker,
}

pub struct Controls {
    level: i32,
    frontlight: bool,
    mode: usize,
    picking: bool,
    /// The percentage, formatted once per change rather than once per frame.
    level_label: String,
}

impl Default for Controls {
    fn default() -> Self {
        Controls::new()
    }
}

impl Controls {
    pub fn new() -> Self {
        Controls {
            level: 60,
            frontlight: true,
            mode: 0,
            picking: false,
            level_label: format!("{}%", 60),
        }
    }

    /// The picker's options, read fresh from the host's table.
    fn modes(&self) -> [&'static str; 2] {
        [tr(c"STR_CONTROLS_FAST"), tr(c"STR_CONTROLS_QUALITY")]
    }

    /// Moves the level, and rewrites its label only when it actually moved.
    ///
    /// The label is a `String`, so writing it on every message would allocate
    /// on opening a dialog and on closing one — neither of which changes a
    /// number.
    fn set_level(&mut self, value: i32) {
        let next = value.clamp(0, 100);
        if next == self.level {
            return;
        }
        self.level = next;
        self.level_label = format!("{next}%");
    }
}

impl Screen for Controls {
    type Message = Msg;

    fn body(&self) -> impl View<Self::Message> {
        NavigationScreen::new(ScrollView::new(vstack![14;
            List::new().push(
                ListRow::new(tr(c"STR_CONTROLS_LEVEL")).value(&self.level_label),
            ),
            // One value, two controls: a plain track and a stepper with a
            // glyph at each end.
            Slider::new(self.level, 100).on_change(Msg::Level),
            Stepper::new(self.level)
                .on_change(Msg::Level)
                .on_step(Msg::StepLevel),

            Divider::new(),

            Toggle::new(
                tr(c"STR_CONTROLS_LIGHT"),
                self.frontlight,
                tr(c"STR_CONTROLS_ON"),
                tr(c"STR_CONTROLS_OFF"),
            )
            .on_change(Msg::Frontlight),

            List::new().push(
                ListRow::new(tr(c"STR_CONTROLS_MODE"))
                    .value(self.modes()[self.mode])
                    .on_tap(Msg::PickMode),
            ),

            ProgressBar::percent(self.level.clamp(0, 100) as u32),
        ]))
        .title(tr(c"STR_CONTROLS_TITLE"))
        .hints(
            Hint::Standard,
            Hint::Standard,
            Hint::text("-"),
            Hint::text("+"),
        )
        // An overlay rather than content: the scroll view must not clip it, and
        // it must not scroll away with the rows underneath.
        .overlay_if(
            self.picking,
            Modal::picker(tr(c"STR_CONTROLS_MODE"), self.modes())
                .selected(self.mode)
                .on_select(Msg::ChoseMode)
                .scrim(Scrim::Dim),
        )
    }

    /// Back closes the picker rather than the screen. The runtime does not
    /// assume this, so a screen showing a dialog has to say so.
    fn on_key(&self, key: Button) -> Option<Msg> {
        match key {
            Button::Back if self.picking => Some(Msg::DismissPicker),
            _ => None,
        }
    }

    fn on_background_tap(&self, _point: xpui::Point) -> Option<Msg> {
        self.picking.then_some(Msg::DismissPicker)
    }

    fn update(&mut self, message: Self::Message) {
        match message {
            Msg::Level(value) => self.set_level(value),
            Msg::StepLevel(delta) => self.set_level(self.level.saturating_add(delta)),
            // The toggle hands over the state it is moving to, so no screen
            // ever writes `!self.something`.
            Msg::Frontlight(next) => self.frontlight = next,
            Msg::PickMode => self.picking = true,
            Msg::ChoseMode(index) => {
                self.mode = index.min(1);
                self.picking = false;
            }
            Msg::DismissPicker => self.picking = false,
        }
    }

    fn title(&self) -> Option<&'static str> {
        Some(tr(c"STR_CONTROLS_TITLE"))
    }
}
