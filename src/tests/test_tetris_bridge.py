"""Тесты Python-моста к нативному тетрису."""

from __future__ import annotations

import unittest
from unittest.mock import patch

from core import Action, FIELD_HEIGHT, FIELD_WIDTH
from brick_game.tetris import c_bridge as tetris_bridge

_NATIVE = tetris_bridge.LIB_PATH.is_file()
_SKIP = (
    f"Нет {tetris_bridge.LIB_PATH.name} — "
    "выполните make в src/native_bridge (или Docker)"
)


@unittest.skipUnless(_NATIVE, _SKIP)
class TetrisBridgeTestCase(unittest.TestCase):
    def setUp(self) -> None:
        self.game = tetris_bridge.TetrisCProxy()

    def test_state_field_dimensions(self) -> None:
        st = self.game.update_current_state()
        self.assertEqual(len(st.field), FIELD_HEIGHT)
        self.assertEqual(len(st.field[0]), FIELD_WIDTH)
        self.assertEqual(len(st.next), FIELD_HEIGHT)
        self.assertIsInstance(st.score, int)

    def test_pause_toggles(self) -> None:
        self.game.user_input(Action.PAUSE, False)
        st = self.game.update_current_state()
        self.assertTrue(st.pause)
        self.game.user_input(Action.PAUSE, False)
        st2 = self.game.update_current_state()
        self.assertFalse(st2.pause)

    def test_start_action_does_not_crash(self) -> None:
        self.game.user_input(Action.START, False)
        st = self.game.update_current_state()
        self.assertIsNotNone(st.field)

    def test_get_render_layers_before_update_returns_empty(self) -> None:
        # Покрываем ветку `if self._last_cstate is None: return {}`.
        layers = self.game.get_render_layers()
        self.assertEqual(layers, {})

    def test_get_render_layers_after_update_contains_game_over(self) -> None:
        _ = self.game.update_current_state()
        layers = self.game.get_render_layers()
        self.assertIn("game_over", layers)


if __name__ == "__main__":
    unittest.main()
