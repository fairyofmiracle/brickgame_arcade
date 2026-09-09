import unittest

from brick_game.server.leaderboard import (
    _connect,
    get_leaderboard,
    init_db,
    submit_score,
    validate_nickname,
)


class LeaderboardTestCase(unittest.TestCase):
    def setUp(self) -> None:
        init_db()
        with _connect() as conn:
            conn.execute("DELETE FROM scores")
            conn.commit()

    def test_validate_nickname(self) -> None:
        self.assertEqual("polyakova_av", validate_nickname("polyakova_av"))
        with self.assertRaises(ValueError):
            validate_nickname("ab")
        with self.assertRaises(ValueError):
            validate_nickname("bad nick")

    def test_submit_and_fetch(self) -> None:
        submit_score("player_one", 4, 42, 3)
        submit_score("player_two", 4, 99, 7)
        data = get_leaderboard(game_id=4, limit=5)
        self.assertEqual(1, len(data["games"]))
        entries = data["games"][0]["entries"]
        self.assertGreaterEqual(len(entries), 2)
        self.assertEqual("player_two", entries[0]["nickname"])
        self.assertEqual(99, entries[0]["score"])
        self.assertEqual(7, entries[0]["level"])

    def test_all_games_leaderboard(self) -> None:
        submit_score("racer", 1, 10)
        submit_score("tetr", 2, 500)
        data = get_leaderboard(limit=3)
        self.assertEqual(4, len(data["games"]))


if __name__ == "__main__":
    unittest.main()
