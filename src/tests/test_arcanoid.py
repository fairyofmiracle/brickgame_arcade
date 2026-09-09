import unittest
import tempfile
from unittest.mock import patch
from pathlib import Path

from core import Action
from brick_game.arcanoid.engine import ArcanoidGame
import brick_game.arcanoid.engine as arcanoid_engine
from brick_game.arcanoid.models import ArcanoidStatus


class ArcanoidGameTestCase(unittest.TestCase):
    def setUp(self) -> None:
        self.game = ArcanoidGame()

    def test_initial_state_is_not_running(self) -> None:
        state = self.game.update_current_state()
        # type: ignore[attr-defined]
        self.assertEqual(ArcanoidStatus.INITIAL, self.game._state.status)
        self.assertEqual(0, state.score)
        self.assertFalse(state.pause)

    def test_start_transitions_to_running(self) -> None:
        self.game.user_input(Action.START, hold=False)
        _ = self.game.update_current_state()
        # type: ignore[attr-defined]
        self.assertEqual(ArcanoidStatus.RUNNING, self.game._state.status)
        # До запуска мяча он "приклеен" к платформе.
        # type: ignore[attr-defined]
        self.assertFalse(self.game._state.ball_released)

    def test_pause_and_resume(self) -> None:
        self.game.user_input(Action.START, hold=False)
        self.game.user_input(Action.PAUSE, hold=False)
        # type: ignore[attr-defined]
        self.assertEqual(ArcanoidStatus.PAUSED, self.game._state.status)
        # type: ignore[attr-defined]
        row_before = self.game._state.ball_row
        self.game.update_current_state()
        # type: ignore[attr-defined]
        self.assertEqual(row_before, self.game._state.ball_row)

        self.game.user_input(Action.PAUSE, hold=False)
        # type: ignore[attr-defined]
        self.assertEqual(ArcanoidStatus.RUNNING, self.game._state.status)

    def test_terminate_sets_game_over(self) -> None:
        self.game.user_input(Action.START, hold=False)
        self.game.user_input(Action.TERMINATE, hold=False)
        _ = self.game.update_current_state()
        # type: ignore[attr-defined]
        self.assertEqual(ArcanoidStatus.GAME_OVER, self.game._state.status)
        # type: ignore[attr-defined]
        self.assertTrue(self.game._state.game_over)

    def test_paddle_moves_within_bounds(self) -> None:
        self.game.user_input(Action.START, hold=False)
        for _ in range(100):
            self.game.user_input(Action.LEFT, hold=False)
        # type: ignore[attr-defined]
        min_center = self.game._min_paddle_center()
        # type: ignore[attr-defined]
        self.assertEqual(min_center, self.game._state.paddle_col)

        for _ in range(100):
            self.game.user_input(Action.RIGHT, hold=False)
        # type: ignore[attr-defined]
        max_center = self.game._max_paddle_center()
        # type: ignore[attr-defined]
        self.assertEqual(max_center, self.game._state.paddle_col)

    def test_action_releases_ball(self) -> None:
        self.game.user_input(Action.START, hold=False)
        self.game.user_input(Action.ACTION, hold=False)
        # type: ignore[attr-defined]
        self.assertTrue(self.game._state.ball_released)

    def test_up_also_releases_ball(self) -> None:
        self.game.user_input(Action.START, hold=False)
        self.game.user_input(Action.UP, hold=False)
        # type: ignore[attr-defined]
        self.assertTrue(self.game._state.ball_released)

    def test_hitting_brick_adds_score_and_updates_level_and_speed(
        self,
    ) -> None:
        self.game.user_input(Action.START, hold=False)
        # type: ignore[attr-defined]
        self.game._state.ball_released = True
        # type: ignore[attr-defined]
        self.game._state.ball_row = 5
        # type: ignore[attr-defined]
        self.game._state.ball_col = 5
        # type: ignore[attr-defined]
        self.game._state.ball_dr = -1
        # type: ignore[attr-defined]
        self.game._state.ball_dc = 0
        # type: ignore[attr-defined]
        self.game._state.score = 4
        # type: ignore[attr-defined]
        self.game._state.level = 1
        # type: ignore[attr-defined]
        self.game._state.high_score = 0
        # type: ignore[attr-defined]
        self.game._state.bricks = {(4, 5)}

        with patch.object(arcanoid_engine, "_store_high_score") as mock_store:
            state = self.game.update_current_state()

        self.assertEqual(5, state.score)
        self.assertEqual(2, state.level)
        self.assertEqual(2, state.speed)
        mock_store.assert_called()

    def test_missing_paddle_causes_game_over(self) -> None:
        self.game.user_input(Action.START, hold=False)
        # type: ignore[attr-defined]
        self.game._state.ball_released = True
        # Шагнет на нижнюю строку вне платформы.
        # type: ignore[attr-defined]
        self.game._state.ball_row = 18
        # type: ignore[attr-defined]
        self.game._state.ball_col = 0
        # type: ignore[attr-defined]
        self.game._state.ball_dr = 1
        # type: ignore[attr-defined]
        self.game._state.ball_dc = 0
        # type: ignore[attr-defined]
        self.game._state.paddle_col = 8

        _ = self.game.update_current_state()
        # type: ignore[attr-defined]
        self.assertEqual(ArcanoidStatus.GAME_OVER, self.game._state.status)
        # type: ignore[attr-defined]
        self.assertTrue(self.game._state.game_over)

    def test_clearing_last_brick_sets_won(self) -> None:
        self.game.user_input(Action.START, hold=False)
        # type: ignore[attr-defined]
        self.game._state.ball_released = True
        # type: ignore[attr-defined]
        self.game._state.ball_row = 5
        # type: ignore[attr-defined]
        self.game._state.ball_col = 5
        # type: ignore[attr-defined]
        self.game._state.ball_dr = -1
        # type: ignore[attr-defined]
        self.game._state.ball_dc = 0
        # type: ignore[attr-defined]
        self.game._state.bricks = {(4, 5)}

        _ = self.game.update_current_state()
        # type: ignore[attr-defined]
        self.assertEqual(ArcanoidStatus.WON, self.game._state.status)
        # type: ignore[attr-defined]
        self.assertTrue(self.game._state.game_over)

    def test_wall_bounce_flips_horizontal_direction(self) -> None:
        self.game.user_input(Action.START, hold=False)
        # type: ignore[attr-defined]
        self.game._state.ball_released = True
        # Bounce from left wall: nc = -1.
        # type: ignore[attr-defined]
        self.game._state.ball_row = 10
        # type: ignore[attr-defined]
        self.game._state.ball_col = 0
        # type: ignore[attr-defined]
        self.game._state.ball_dr = 0
        # type: ignore[attr-defined]
        self.game._state.ball_dc = -1
        # Keep at least one brick so we don't auto-win.
        # type: ignore[attr-defined]
        self.game._state.bricks = {(2, 2)}

        _ = self.game.update_current_state()
        # type: ignore[attr-defined]
        self.assertEqual(1, self.game._state.ball_dc)
        # type: ignore[attr-defined]
        self.assertEqual(1, self.game._state.ball_col)

    def test_ceiling_bounce_flips_vertical_direction(self) -> None:
        self.game.user_input(Action.START, hold=False)
        # type: ignore[attr-defined]
        self.game._state.ball_released = True
        # Bounce from ceiling: nr = -1.
        # type: ignore[attr-defined]
        self.game._state.ball_row = 0
        # type: ignore[attr-defined]
        self.game._state.ball_col = 5
        # type: ignore[attr-defined]
        self.game._state.ball_dr = -1
        # type: ignore[attr-defined]
        self.game._state.ball_dc = 0
        # type: ignore[attr-defined]
        self.game._state.bricks = {(2, 2)}

        _ = self.game.update_current_state()
        # type: ignore[attr-defined]
        self.assertEqual(1, self.game._state.ball_dr)
        # type: ignore[attr-defined]
        self.assertEqual(1, self.game._state.ball_row)

    def test_horizontal_brick_hit_flips_dc(self) -> None:
        self.game.user_input(Action.START, hold=False)
        # type: ignore[attr-defined]
        self.game._state.ball_released = True
        # curr=(5,5), dr=0, dc=+1 => nr=5, nc=6; horiz_hit=(5,6).
        # type: ignore[attr-defined]
        self.game._state.ball_row = 5
        # type: ignore[attr-defined]
        self.game._state.ball_col = 5
        # type: ignore[attr-defined]
        self.game._state.ball_dr = 0
        # type: ignore[attr-defined]
        self.game._state.ball_dc = 1
        # type: ignore[attr-defined]
        self.game._state.bricks = {(5, 6)}

        _ = self.game.update_current_state()
        # type: ignore[attr-defined]
        self.assertEqual(-1, self.game._state.ball_dc)
        # brick removed
        # type: ignore[attr-defined]
        self.assertNotIn((5, 6), self.game._state.bricks)

    def test_diagonal_brick_hit_flips_dr_when_no_other_hit(self) -> None:
        self.game.user_input(Action.START, hold=False)
        # type: ignore[attr-defined]
        self.game._state.ball_released = True
        # curr=(5,5), dr=-1, dc=+1 => nr=4, nc=6; diag_hit=(4,6).
        # Ensure no vert_hit=(4,5) and no horiz_hit=(5,6).
        # type: ignore[attr-defined]
        self.game._state.ball_row = 5
        # type: ignore[attr-defined]
        self.game._state.ball_col = 5
        # type: ignore[attr-defined]
        self.game._state.ball_dr = -1
        # type: ignore[attr-defined]
        self.game._state.ball_dc = 1
        # type: ignore[attr-defined]
        self.game._state.bricks = {(4, 6)}

        _ = self.game.update_current_state()
        # type: ignore[attr-defined]
        self.assertEqual(1, self.game._state.ball_dr)
        # type: ignore[attr-defined]
        self.assertNotIn((4, 6), self.game._state.bricks)

    def test_paddle_forgiveness_zone_counts_as_hit_and_steers_ball(
        self,
    ) -> None:
        self.game.user_input(Action.START, hold=False)
        # type: ignore[attr-defined]
        self.game._state.ball_released = True
        # Paddle center at 5 => cols {4,5,6}, min=4 so forgiveness is nc==3.
        # type: ignore[attr-defined]
        self.game._state.paddle_col = 5
        # Step onto paddle row (19): ball_row=18, dr=+1.
        # Choose ball_col=2, dc=+1 => nc=3 (min-1).
        # type: ignore[attr-defined]
        self.game._state.ball_row = 18
        # type: ignore[attr-defined]
        self.game._state.ball_col = 2
        # type: ignore[attr-defined]
        self.game._state.ball_dr = 1
        # type: ignore[attr-defined]
        self.game._state.ball_dc = 1
        # Keep a brick so we don't win.
        # type: ignore[attr-defined]
        self.game._state.bricks = {(2, 2)}

        _ = self.game.update_current_state()
        # Should bounce upward and steer left (nc < paddle_col).
        # type: ignore[attr-defined]
        self.assertEqual(-1, self.game._state.ball_dr)
        # type: ignore[attr-defined]
        self.assertEqual(-1, self.game._state.ball_dc)
        # nc is clamped into paddle range, so ball_col ends up within [4,6].
        # type: ignore[attr-defined]
        self.assertIn(self.game._state.ball_col, (4, 5, 6))

    def test_paddle_right_forgiveness_zone_sets_ball_dc_positive(self) -> None:
        self.game.user_input(Action.START, hold=False)
        # type: ignore[attr-defined]
        self.game._state.ball_released = True
        # Paddle center at 5 => cols {4,5,6}, max=6 so forgiveness is nc==7.
        # Make nc==7 with a right-side hit and start with dc=-1 so we can
        # observe it flipping to +1.
        # type: ignore[attr-defined]
        self.game._state.paddle_col = 5
        # type: ignore[attr-defined]
        self.game._state.ball_row = 18
        # type: ignore[attr-defined]
        self.game._state.ball_col = 8
        # type: ignore[attr-defined]
        self.game._state.ball_dr = 1
        # type: ignore[attr-defined]
        self.game._state.ball_dc = -1
        # type: ignore[attr-defined]
        self.game._state.bricks = {(2, 2)}

        _ = self.game.update_current_state()
        # type: ignore[attr-defined]
        self.assertEqual(-1, self.game._state.ball_dr)
        # type: ignore[attr-defined]
        self.assertEqual(1, self.game._state.ball_dc)

    def test_get_render_layers_contains_expected_fields(self) -> None:
        self.game.user_input(Action.START, hold=False)
        layers = self.game.get_render_layers()
        self.assertIn("game_over", layers)
        self.assertIn("status", layers)
        self.assertIn("lives", layers)

    def test_high_score_load_and_store_exception_paths(self) -> None:
        with patch.object(
            arcanoid_engine,
            "_ARCANOID_HIGH_SCORE_FILE",
            new=arcanoid_engine.SCORES_DIR,
        ):
            game2 = arcanoid_engine.ArcanoidGame()  # type: ignore[call-arg]
            # type: ignore[attr-defined]
            self.assertEqual(0, game2._state.high_score)

        with patch.object(
            arcanoid_engine, "ensure_scores_dir", side_effect=OSError("boom")
        ):
            arcanoid_engine._store_high_score(123)

    def test_store_high_score_writes_file_on_success(self) -> None:
        # Cover the successful open/write path in _store_high_score().
        with tempfile.TemporaryDirectory() as td:
            scores_dir = Path(td)
            hs_path = scores_dir / "high_score_arcanoid.txt"
            with patch.object(arcanoid_engine, "SCORES_DIR", new=scores_dir):
                with patch.object(
                    arcanoid_engine,
                    "_ARCANOID_HIGH_SCORE_FILE",
                    new=hs_path,
                ):
                    with patch.object(
                        arcanoid_engine,
                        "ensure_scores_dir",
                        return_value=scores_dir,
                    ):
                        arcanoid_engine._store_high_score(777)
            self.assertTrue(hs_path.exists())
            self.assertEqual("777", hs_path.read_text(encoding="utf-8"))

    def test_user_input_ignored_when_not_running_or_paused(self) -> None:
        # INITIAL status: movement inputs should be ignored.
        # type: ignore[attr-defined]
        before = self.game._state.paddle_col
        self.game.user_input(Action.LEFT, hold=False)
        # type: ignore[attr-defined]
        self.assertEqual(before, self.game._state.paddle_col)

    def test_update_current_state_breaks_when_game_ends_mid_steps(
        self,
    ) -> None:
        self.game.user_input(Action.START, hold=False)
        # Make steps > 1.
        # type: ignore[attr-defined]
        self.game._state.level = 9
        # type: ignore[attr-defined]
        self.game._state.game_over = False
        # type: ignore[attr-defined]
        self.game._state.pause = False

        calls = {"n": 0}

        def end_game_after_first_step() -> None:
            calls["n"] += 1
            # type: ignore[attr-defined]
            self.game._state.game_over = True

        # Patch the internal method called from update_current_state().
        # mypy: dynamic patching for test purposes
        self.game._step_ball = end_game_after_first_step  # type: ignore
        _ = self.game.update_current_state()
        self.assertEqual(1, calls["n"])


if __name__ == "__main__":
    unittest.main()
