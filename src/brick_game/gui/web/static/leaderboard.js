// School nick + server leaderboard helpers.

const NICK_KEY = 'brickgame.school_nick';

export function getSchoolNick() {
    try {
        return String(localStorage.getItem(NICK_KEY) || '').trim().slice(0, 20);
    } catch {
        return '';
    }
}

export function setSchoolNick(value) {
    try {
        const v = String(value || '').trim().slice(0, 20);
        if (v) localStorage.setItem(NICK_KEY, v);
        else localStorage.removeItem(NICK_KEY);
    } catch {
        // ignore
    }
}

export function isValidSchoolNick(value) {
    const v = String(value || '').trim();
    return /^[a-zA-Z0-9_]{3,20}$/.test(v);
}

export async function fetchLeaderboard(apiBaseUrl, limit = 5) {
    const res = await fetch(
        `${apiBaseUrl}/api/leaderboard?limit=${encodeURIComponent(limit)}`,
    );
    const json = await res.json().catch(() => ({}));
    if (!res.ok) {
        throw new Error(json?.message || `HTTP ${res.status}`);
    }
    return Array.isArray(json?.games) ? json.games : [];
}

export async function submitLeaderboardScore(
    apiBaseUrl,
    nickname,
    gameId,
    score,
    level = 1,
) {
    const res = await fetch(`${apiBaseUrl}/api/leaderboard`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
            nickname,
            game_id: Number(gameId),
            score: Number(score),
            level: Math.max(1, Number(level) || 1),
        }),
    });
    const json = await res.json().catch(() => ({}));
    if (!res.ok) {
        throw new Error(json?.message || `HTTP ${res.status}`);
    }
    return json;
}
