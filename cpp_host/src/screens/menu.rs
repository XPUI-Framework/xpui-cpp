//! The root screen, and what each row opens.
//!
//! Every label here comes through [`tr`](crate::strings::tr), so the screen
//! reads the host's string table rather than carrying English of its own —
//! which is what a firmware does, and the reason an unknown key shows up as
//! the key itself rather than as a blank row.

use xpui::screen::Screen;
use xpui::{List, ListRow, NavigationScreen, View, present};

use crate::screens::{About, Controls};
use crate::strings::tr;

/// What the menu can open.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub enum Example {
    About,
    Controls,
}

/// The screen the host starts on.
///
/// Stateless: a row's whole effect is a push, which crosses the FFI and
/// becomes a screen on the C++ stack. What proves it happened is the stack's
/// own depth, asserted by the `navigates_into_rust` case in `ctest`.
pub struct Menu;

impl Default for Menu {
    fn default() -> Self {
        Menu::new()
    }
}

impl Menu {
    pub fn new() -> Self {
        Menu
    }
}

impl Screen for Menu {
    type Message = Example;

    fn body(&self) -> impl View<Self::Message> {
        NavigationScreen::new(
            List::new()
                .push(
                    ListRow::new(tr(c"STR_MENU_ABOUT"))
                        .subtitle(tr(c"STR_MENU_ABOUT_SUB"))
                        .on_tap(Example::About),
                )
                .push(
                    ListRow::new(tr(c"STR_MENU_CONTROLS"))
                        .subtitle(tr(c"STR_MENU_CONTROLS_SUB"))
                        .on_tap(Example::Controls),
                ),
        )
        .title(tr(c"STR_MENU_TITLE"))
    }

    fn update(&mut self, message: Self::Message) {
        // `present` hands the screen to whatever navigator is installed. Here
        // that is `Shell`, so it leaves Rust as an opaque handle and comes back
        // as an entry on the C++ stack.
        match message {
            Example::About => present(About::new()),
            Example::Controls => present(Controls::new()),
        };
    }

    fn title(&self) -> Option<&'static str> {
        Some(tr(c"STR_MENU_TITLE"))
    }
}
