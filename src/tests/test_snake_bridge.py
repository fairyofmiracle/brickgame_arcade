"""Тесты Python-моста к нативной змейке (нужен собранный libbrickgames.so)."""

from __future__ import annotations

import unittest
from unittest.mock import patch

from core import Action, FIELD_HEIGHT, FIELD_WIDTH
from brick_game.snake import c_bridge as snake_bridge

_NATIVE = snake_bridge.LIB_PATH.is_file()
_SKIP = (
    f"Нет {snake_bridge.LIB_PATH.name} — "
    "выполните make в src/native_bridge (или Docker)"
)


@unittest.skipUnless(_NATIVE, _SKIP)
class SnakeBridgeTestCase(unittest.TestCase):
    def setUp(self) -> None:
        self.game = snake_bridge.SnakeCProxy()

    def test_state_field_dimensions(self) -> None:
        st = self.game.update_current_state()
        self.assertEqual(len(st.field), FIELD_HEIGHT)
        self.assertEqual(len(st.field[0]), FIELD_WIDTH)
        self.assertIsInstance(st.score, int)

    def test_pause_toggles_via_api(self) -> None:
        """Пауза из REST/Python (action_id=1), не только клавиша 'p'."""
        self.game.user_input(Action.UP, False)
        self.game.update_current_state()

        self.game.user_input(Action.PAUSE, False)
        st = self.game.update_current_state()
        self.assertTrue(st.pause)

        self.game.user_input(Action.PAUSE, False)
        st2 = self.game.update_current_state()
        self.assertFalse(st2.pause)

    def test_direction_input_after_init(self) -> None:
        self.game.user_input(Action.RIGHT, False)
        st = self.game.update_current_state()
        # поле не пустое: есть змейка и/или еда
        filled = sum(1 for row in st.field for c in row if c)
        self.assertGreater(filled, 0)

    def test_get_render_layers_before_update_returns_empty(self) -> None:
        # Покрываем ветку `if self._last_cstate is None: return {}`.
        layers = self.game.get_render_layers()
        self.assertEqual(layers, {})

    def test_unsupported_action_is_ignored(self) -> None:
        # `Action.START`/`Action.TERMINATE`
        # не мапятся в ACTION_MAP для движения,
        # поэтому proxy должен просто игнорировать.
        self.game.user_input(Action.START, False)
        self.game.user_input(Action.TERMINATE, False)
        st = self.game.update_current_state()
        self.assertIsInstance(st.score, int)

    def test_get_render_layers_after_update_has_colors(self) -> None:
        _ = self.game.update_current_state()
        layers = self.game.get_render_layers()
        self.assertIn("field_colors", layers)
        self.assertIn("next_colors", layers)


if __name__ == "__main__":
    unittest.main()
