import unittest
import runpy
import sys

from fastapi.testclient import TestClient

from unittest.mock import patch


class ServerApiTestCase(unittest.TestCase):
    def setUp(self) -> None:
        # Local import: avoid runpy warning in __main__ test.
        import brick_game.server.app as server_app

        self.server_app = server_app
        self.client = TestClient(server_app.app)

    def tearDown(self) -> None:
        # Сбрасываем текущую игру между тестами, чтобы не было влияния друг на
        # друга.
        self.server_app._current_game = None  # type: ignore[attr-defined]
        self.server_app._current_game_id = None  # type: ignore[attr-defined]

    def test_list_games_returns_three_entries(self) -> None:
        resp = self.client.get("/games")
        self.assertEqual(200, resp.status_code)
        data = resp.json()
        self.assertIn("games", data)
        self.assertGreaterEqual(len(data["games"]), 3)

    def test_select_race_game_and_get_state(self) -> None:
        # Выбор игры гонки (id=1)
        resp_sel = self.client.post("/games/1")
        self.assertEqual(200, resp_sel.status_code)

        # Состояние после старта
        resp_state = self.client.get("/state")
        self.assertEqual(200, resp_state.status_code)
        state = resp_state.json()
        self.assertIn("field", state)
        self.assertIn("score", state)
        self.assertIsInstance(state["field"], list)

    def test_select_other_game_returns_409(self) -> None:
        # Запускаем гонки (id=1)
        resp_sel = self.client.post("/games/1")
        self.assertEqual(200, resp_sel.status_code)

        # Пытаемся запустить тетрис поверх активной игры
        resp_conflict = self.client.post("/games/2")
        self.assertEqual(409, resp_conflict.status_code)

        # Активная игра должна остаться гонками
        resp_state = self.client.get("/state")
        self.assertEqual(200, resp_state.status_code)
        state = resp_state.json()
        self.assertIn("lives", state)

    def test_api_prefixed_routes_work(self) -> None:
        resp_games = self.client.get("/api/games")
        self.assertEqual(200, resp_games.status_code)
        data = resp_games.json()
        self.assertIn("games", data)

        resp_sel = self.client.post("/api/games/1")
        self.assertEqual(200, resp_sel.status_code)

        resp_state = self.client.get("/api/state")
        self.assertEqual(200, resp_state.status_code)
        state = resp_state.json()
        self.assertIn("field", state)
        self.assertIn("score", state)

    def test_actions_require_started_game(self) -> None:
        # Без запуска игры ожидаем 400
        resp = self.client.post(
            "/actions", json={"action_id": 3, "hold": False})
        self.assertEqual(400, resp.status_code)

        # Запускаем игру и пробуем ещё раз
        self.client.post("/games/1")
        resp_ok = self.client.post(
            "/actions", json={"action_id": 3, "hold": False})
        self.assertEqual(200, resp_ok.status_code)

    def test_state_requires_started_game(self) -> None:
        resp = self.client.get("/state")
        self.assertEqual(400, resp.status_code)

    def test_actions_invalid_action_id_returns_400(self) -> None:
        self.client.post("/games/1")
        resp = self.client.post(
            "/actions", json={"action_id": 999, "hold": False})
        self.assertEqual(400, resp.status_code)

    def test_actions_body_validation_error_returns_400_and_message(
        self,
    ) -> None:
        # No game started + invalid body should trigger RequestValidationError
        # handler (400) with our ErrorMessage schema.
        resp = self.client.post("/actions", json={"action_id": "oops"})
        self.assertEqual(400, resp.status_code)
        data = resp.json()
        self.assertIn("message", data)
        # Message is aggregated from validation errors.
        self.assertTrue(isinstance(data["message"], str) and data["message"])

    def test_select_invalid_game_returns_404(self) -> None:
        resp = self.client.post("/games/999")
        self.assertEqual(404, resp.status_code)

    def test_index_endpoint_returns_html(self) -> None:
        resp = self.client.get("/")
        self.assertEqual(200, resp.status_code)
        self.assertIn("BrickGame", resp.text)

    def test_state_snake_includes_field_and_next_colors(self) -> None:
        # Snake = id 3
        self.client.post("/games/3")
        resp_state = self.client.get("/state")
        self.assertEqual(200, resp_state.status_code)
        data = resp_state.json()
        self.assertIn("field_colors", data)
        self.assertIn("next_colors", data)

    def test_state_tetris_selection_works(self) -> None:
        # Tetris = id 2
        self.client.post("/games/2")
        resp_state = self.client.get("/state")
        self.assertEqual(200, resp_state.status_code)
        data = resp_state.json()
        self.assertIn("field", data)

    def test_state_arcanoid_includes_arcanoid_render_layer(self) -> None:
        # Arcanoid = id 4
        resp_sel = self.client.post("/games/4")
        self.assertEqual(200, resp_sel.status_code)
        resp_state = self.client.get("/state")
        self.assertEqual(200, resp_state.status_code)
        data = resp_state.json()
        self.assertIn("arcanoid", data)
        arc = data["arcanoid"]
        self.assertIsInstance(arc, dict)
        self.assertIn("ball", arc)
        self.assertIn("paddle", arc)
        self.assertIn("bricks", arc)

    def test_get_state_handles_exception_in_get_render_layers(self) -> None:
        # Сначала запустим игру, потом заставим get_render_layers бросать
        # исключение.
        self.client.post("/games/1")

        # server_app._current_game гарантированно существует после select_game.
        cg = self.server_app._current_game  # type: ignore[attr-defined]
        assert cg is not None

        def boom() -> dict:
            raise RuntimeError("boom")

        cg.get_render_layers = boom  # type: ignore[method-assign]

        resp_state = self.client.get("/state")
        self.assertEqual(200, resp_state.status_code)

    def test_create_app_handles_chdir_error(self) -> None:
        # Покрываем except OSError в create_app(), принудительно ломая
        # os.chdir.
        with patch.object(
            self.server_app.os,
            "chdir",
            side_effect=OSError("boom"),
        ):
            new_app = self.server_app.create_app()
            self.assertIsNotNone(new_app)

    def test_delete_games_clears_current_game(self) -> None:
        self.client.post("/games/1")
        resp_del = self.client.delete("/games")
        self.assertEqual(200, resp_del.status_code)

        resp_sel = self.client.post("/games/2")
        self.assertEqual(200, resp_sel.status_code)

    def test_delete_api_games_clears_current_game(self) -> None:
        self.client.post("/api/games/1")
        resp_del = self.client.delete("/api/games")
        self.assertEqual(200, resp_del.status_code)

        resp_sel = self.client.post("/api/games/3")
        self.assertEqual(200, resp_sel.status_code)

    def test_api_actions_endpoint_works(self) -> None:
        self.client.post("/api/games/1")
        resp = self.client.post(
            "/api/actions", json={"action_id": 3, "hold": False})
        self.assertEqual(200, resp.status_code)

    def test_main_block_invokes_uvicorn_run(self) -> None:
        # Покрываем строки в __main__-блоке, но не запускаем реальный сервер.
        with patch("uvicorn.run") as mock_run:
            # runpy warns if the module is already imported.
            sys.modules.pop("brick_game.server.app", None)
            runpy.run_module("brick_game.server.app", run_name="__main__")
            self.assertTrue(mock_run.called)

    def test_error_body_handles_non_dict_list_items_and_fallback(self) -> None:
        # Cover rare paths in _error_body().
        from brick_game.server.app import _error_body

        msg = _error_body([{"loc": ("x",), "msg": "bad"}, 123])["message"]
        self.assertIn("x: bad", msg)
        self.assertIn("123", msg)

        self.assertEqual({"message": "42"}, _error_body(42))


if __name__ == "__main__":
    unittest.main()
