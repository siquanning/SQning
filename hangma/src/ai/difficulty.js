// ============================================================
// difficulty.js — AI difficulty presets
// ============================================================

/** @type {Record<string, { iterations: number, randomRate: number }>} */
export const DIFFICULTY = {
  easy:   { iterations: 500,  randomRate: 0.15 },
  normal: { iterations: 1500, randomRate: 0.05 },
  hard:   { iterations: 3000, randomRate: 0.00 },
};

/**
 * @param {string} level — 'easy' | 'normal' | 'hard'
 * @returns {{ iterations: number, randomRate: number }}
 */
export function getDifficultyConfig(level) {
  return DIFFICULTY[level] || DIFFICULTY.normal;
}
