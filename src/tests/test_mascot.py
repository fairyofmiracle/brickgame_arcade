import unittest

from brick_game.server.mascot import MascotSayRequest, mascot_say


class MascotFallbackTestCase(unittest.IsolatedAsyncioTestCase):
    async def test_fallback_menu_hello(self) -> None:
        req = MascotSayRequest(nickname="polyakova", event="menu_hello")
        lines, source = await mascot_say(req, client_ip="test-1")
        self.assertEqual("fallback", source)
        self.assertGreaterEqual(len(lines), 1)
        self.assertLessEqual(len(lines), 3)

    async def test_fallback_arcanoid_hint(self) -> None:
        req = MascotSayRequest(
            nickname="player",
            game_id=4,
            event="hint",
            screen="playing",
            level=3,
            score=12,
        )
        lines, source = await mascot_say(req, client_ip="test-2")
        self.assertEqual("fallback", source)
        self.assertTrue(all(len(ln) <= 42 for ln in lines))

    async def test_rate_limit_uses_fallback(self) -> None:
        ip = "rate-test-ip"
        for _ in range(25):
            await mascot_say(
                MascotSayRequest(event="hint"),
                client_ip=ip,
            )
        lines, source = await mascot_say(
            MascotSayRequest(event="hint"),
            client_ip=ip,
        )
        self.assertEqual("fallback", source)
        self.assertGreaterEqual(len(lines), 1)


if __name__ == "__main__":
    unittest.main()
