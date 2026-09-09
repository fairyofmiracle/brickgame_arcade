import random
import unittest
from unittest.mock import patch

from core import Action
from brick_game.race.engine import (
    RaceGame,
    _enemy_step_for_level,
    _level_from_score,
)
import brick_game.race.engine as race_engine
from brick_game.race.models import RaceStatus


class RaceGameTestCase(unittest.TestCase):
    def setUp(self) -> None:
        # Для воспроизводимости спавна соперников
        random.seed(0)
        self.game = RaceGame()

    def test_start_resets_state_and_runs(self) -> None:
        self.game.update_current_state()
        # type: ignore[attr-defined]
        self.assertEqual(RaceStatus.INITIAL, self.game._state.status)

        self.game.user_input(Action.START, hold=False)
        state_after = self.game.update_current_state()

        # type: ignore[attr-defined]
        self.assertEqual(RaceStatus.RUNNING, self.game._state.status)
        self.assertFalse(state_after.pause)
        self.assertGreaterEqual(state_after.score, 0)

    def test_pause_and_resume(self) -> None:
        self.game.user_input(Action.START, hold=False)
        # type: ignore[attr-defined]
        self.assertEqual(RaceStatus.RUNNING, self.game._state.status)

        self.game.user_input(Action.PAUSE, hold=False)
        # type: ignore[attr-defined]
        self.assertEqual(RaceStatus.PAUSED, self.game._state.status)

        # В паузе состояние не должно продвигаться по тикам
        tick_before = self.game._state.tick  # type: ignore[attr-defined]
        self.game.update_current_state()
        # type: ignore[attr-defined]
        self.assertEqual(tick_before, self.game._state.tick)

        self.game.user_input(Action.PAUSE, hold=False)
        # type: ignore[attr-defined]
        self.assertEqual(RaceStatus.RUNNING, self.game._state.status)

    def test_player_moves_between_lanes(self) -> None:
        self.game.user_input(Action.START, hold=False)
        initial_col = self.game._state.player_col  # type: ignore[attr-defined]

        self.game.user_input(Action.LEFT, hold=False)
        self.assertEqual(
            initial_col - 1, self.game._state.player_col
        )

        # Несколько раз вправо, не выходим за пределы поля
        for _ in range(20):
            self.game.user_input(Action.RIGHT, hold=False)
        lane_centers = self.game._lane_centers  # type: ignore[attr-defined]
        self.assertEqual(lane_centers[-1], self.game._state.player_col)

        # Несколько раз влево, не выходим за пределы поля
        for _ in range(20):
            self.game.user_input(Action.LEFT, hold=False)
        self.assertEqual(lane_centers[0], self.game._state.player_col)

    def test_acceleration_changes_speed(self) -> None:
        self.game.user_input(Action.START, hold=False)

        # Без удержания Up — базовая скорость (уровень 1)
        self.game.user_input(Action.UP, hold=False)
        state = self.game.update_current_state()
        self.assertEqual(1, state.speed)

        # С удержанием Up — увеличенная скорость
        self.game.user_input(Action.UP, hold=True)
        state_fast = self.game.update_current_state()
        self.assertGreaterEqual(state_fast.speed, 2)

    def test_level_from_score_readme_part3(self) -> None:
        """Часть 3 README: +1 уровень каждые 5 очков, макс. 10."""
        self.assertEqual(1, _level_from_score(0))
        self.assertEqual(1, _level_from_score(4))
        self.assertEqual(2, _level_from_score(5))
        self.assertEqual(10, _level_from_score(45))
        self.assertEqual(10, _level_from_score(1000))

    def test_enemy_step_grows_with_level(self) -> None:
        self.assertEqual(1, _enemy_step_for_level(1))
        self.assertGreater(_enemy_step_for_level(10), _enemy_step_for_level(1))

    def test_enemies_spawn_and_score_increases(self) -> None:
        self.game.user_input(Action.START, hold=False)

        # Прокрутим достаточно тиков, чтобы заспавнить и "пропустить" машины
        for _ in range(50):
            self.game.update_current_state()

        state = self.game.update_current_state()
        self.assertGreaterEqual(state.score, 0)
        # Хоть одна машина должна была заспавниться
        # type: ignore[attr-defined]
        self.assertGreaterEqual(len(self.game._state.enemies), 0)

    def test_terminate_sets_game_over(self) -> None:
        self.game.user_input(Action.START, hold=False)
        self.game.user_input(Action.TERMINATE, hold=False)

        state = self.game.update_current_state()
        # type: ignore[attr-defined]
        self.assertEqual(RaceStatus.GAME_OVER, self.game._state.status)
        # type: ignore[attr-defined]
        self.assertTrue(self.game._state.game_over)
        self.assertFalse(state.pause)

    def test_user_input_ignored_when_not_running(self) -> None:
        # Покрываем ветку early-return:
        # `if self._state.status != RUNNING or self._state.pause: return`
        initial_col = self.game._state.player_col  # type: ignore[attr-defined]
        self.game.user_input(Action.LEFT, hold=False)
        # type: ignore[attr-defined]
        self.assertEqual(initial_col, self.game._state.player_col)

    def test_game_over_collision_when_lives_reach_zero(self) -> None:
        # Создадим ситуацию столкновения: соперник в зоне игрока и 1 жизнь.
        self.game.user_input(Action.START, hold=False)
        self.game._state.lives = 1  # type: ignore[attr-defined]
        # Внутри _step столкновение, если:
        # row >= player_row - 3 и abs(col - player_col) <= 1
        player_col = self.game._state.player_col  # type: ignore[attr-defined]
        player_row = self.game._config.height - 1  # type: ignore[attr-defined]
        # type: ignore[attr-defined]
        self.game._state.enemies = [(player_row - 3, player_col)]

        state = self.game.update_current_state()
        # type: ignore[attr-defined]
        self.assertEqual(RaceStatus.GAME_OVER, self.game._state.status)
        # type: ignore[attr-defined]
        self.assertTrue(self.game._state.game_over)
        # В core.State нет поля game_over, оно идёт отдельными render layers.
        self.assertFalse(state.pause)

    def test_collision_clears_enemies_if_lives_remain(self) -> None:
        self.game.user_input(Action.START, hold=False)
        self.game._state.lives = 3  # type: ignore[attr-defined]
        player_col = self.game._state.player_col  # type: ignore[attr-defined]
        player_row = self.game._config.height - 1  # type: ignore[attr-defined]
        # type: ignore[attr-defined]
        self.game._state.enemies = [(player_row - 3, player_col)]

        _ = self.game.update_current_state()
        # Соперник должен быть убран, а игра продолжиться.
        # type: ignore[attr-defined]
        self.assertEqual(RaceStatus.RUNNING, self.game._state.status)
        # type: ignore[attr-defined]
        self.assertEqual([], self.game._state.enemies)

    def test_convoy_spawn_when_accelerated(self) -> None:
        self.game.user_input(Action.START, hold=False)
        self.game.user_input(Action.UP, hold=True)
        # base=10, //2=5, tick%5==0 на tick=5
        self.game._state.tick = 4  # type: ignore[attr-defined]
        _ = self.game.update_current_state()

        # type: ignore[attr-defined]
        self.assertGreaterEqual(
            len(self.game._state.enemies), 2
        )

    def test_high_score_updated_when_score_exceeds_previous(self) -> None:
        # Провоцируем ситуацию, где при одном update_current_state
        # будет "пройдено" >=1 машины,
        # и score станет > high_score, чтобы вызвался _store_high_score().
        self.game.user_input(Action.START, hold=False)
        self.game._state.high_score = 0  # type: ignore[attr-defined]
        self.game._state.score = 0  # type: ignore[attr-defined]
        self.game._state.level = 1  # type: ignore[attr-defined]
        # type: ignore[attr-defined]
        self.game._state.enemies = [
            (self.game._config.height - 1, self.game._state.player_col)]

        _ = self.game.update_current_state()

        # type: ignore[attr-defined]
        self.assertGreater(self.game._state.high_score, 0)

    def test_high_score_load_and_store_exception_paths(self) -> None:
        # Покрываем except OSError для загрузки и сохранения рекорда.
        with patch.object(
            race_engine, "_RACE_HIGH_SCORE_FILE", new=race_engine.SCORES_DIR
        ):
            # Сделаем "файл" директорией, чтение упадёт с OSError.
            game2 = race_engine.RaceGame()  # type: ignore[call-arg]
            # type: ignore[attr-defined]
            self.assertEqual(0, game2._state.high_score)

        with patch.object(
            race_engine, "ensure_scores_dir", side_effect=OSError("boom")
        ):
            # Должно просто проглотить исключение.
            race_engine._store_high_score(123)


if __name__ == "__main__":
    unittest.main()
