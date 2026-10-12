/*
 * File circulaire mono-producteur / mono-consommateur des fronts SDA/SCL.
 * Producteur : routine d'interruption GPIO (en IRAM). Consommateur : tâche
 * de décodage. Pas d'opération atomique lecture-modification-écriture
 * (l'ESP32-C3 est RV32IMC, sans extension A) : seuls des chargements et
 * stockages 32 bits ordonnés sont utilisés.
 */
#ifndef EDGE_RING_H
#define EDGE_RING_H

#include <stdint.h>

#ifndef EDGE_RING_SIZE
#define EDGE_RING_SIZE 2048u
#endif
_Static_assert((EDGE_RING_SIZE & (EDGE_RING_SIZE - 1u)) == 0, "EDGE_RING_SIZE doit etre une puissance de 2");

#define EDGE_LV_SCL 0x01u
#define EDGE_LV_SDA 0x02u

#define N2_ALWAYS_INLINE static inline __attribute__((always_inline))

typedef struct {
    uint32_t t;   /* horodatage brut (cycles CPU sur cible, µs sur hôte) */
    uint32_t lv;  /* EDGE_LV_SCL | EDGE_LV_SDA */
} edge_rec_t;

typedef struct {
    edge_rec_t buf[EDGE_RING_SIZE];
    volatile uint32_t w;        /* écrit par le producteur seul */
    volatile uint32_t r;        /* écrit par le consommateur seul */
    volatile uint32_t dropped;  /* écrit par le producteur seul */
} edge_ring_t;

N2_ALWAYS_INLINE void edge_ring_reset(edge_ring_t *q)
{
    q->w = 0;
    q->r = 0;
    q->dropped = 0;
}

/* Retourne 1 si le front est enregistré, 0 s'il est perdu (file pleine). */
N2_ALWAYS_INLINE int edge_ring_push(edge_ring_t *q, uint32_t t, uint32_t lv)
{
    uint32_t w = q->w;
    uint32_t r = __atomic_load_n(&q->r, __ATOMIC_ACQUIRE);
    if (w - r >= EDGE_RING_SIZE) {
        q->dropped = q->dropped + 1u;
        return 0;
    }
    q->buf[w & (EDGE_RING_SIZE - 1u)].t = t;
    q->buf[w & (EDGE_RING_SIZE - 1u)].lv = lv;
    __atomic_store_n(&q->w, w + 1u, __ATOMIC_RELEASE);
    return 1;
}

N2_ALWAYS_INLINE int edge_ring_pop(edge_ring_t *q, edge_rec_t *out)
{
    uint32_t r = q->r;
    uint32_t w = __atomic_load_n(&q->w, __ATOMIC_ACQUIRE);
    if (r == w) {
        return 0;
    }
    *out = q->buf[r & (EDGE_RING_SIZE - 1u)];
    __atomic_store_n(&q->r, r + 1u, __ATOMIC_RELEASE);
    return 1;
}

N2_ALWAYS_INLINE uint32_t edge_ring_dropped(const edge_ring_t *q)
{
    return __atomic_load_n(&q->dropped, __ATOMIC_ACQUIRE);
}

#endif
