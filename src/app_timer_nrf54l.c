/**
 * app_timer implementation for nRF54L using hardware TIMER peripheral.
 *
 * Replaces the SDK's RTC1-based app_timer.c since nRF54L has no RTC
 * (it uses GRTC which has a completely different register interface).
 *
 * Uses TIMER20 in 24-bit mode with prescaler to approximate the 32768 Hz
 * tick rate of the original RTC1-based implementation.
 *
 * TIMER20 @ 128 MHz / 2^12 = 31250 Hz (close to 32768 Hz)
 * Uses CC[0] for compare event, CC[1] for counter capture.
 *
 * This is an MVP implementation sufficient for bootloader use:
 * - Single-shot and repeating timers
 * - app_scheduler integration
 * - Limited number of concurrent timers (bootloader uses 1-2)
 */

#include "sdk_common.h"
#if NRF_MODULE_ENABLED(APP_TIMER)
#include "app_timer.h"
#include <stdlib.h>
#include "nrf.h"
#include "nrf_soc.h"
#include "app_error.h"
#include "nrf_delay.h"
#include "nrf_timer.h"
#include "app_util_platform.h"

#if APP_TIMER_CONFIG_USE_SCHEDULER
#include "app_scheduler.h"
#endif

/* TIMER peripheral used for app_timer */
#define TIMER_INST          NRF_TIMER20
#define TIMER_IRQn_INST     TIMER20_IRQn
#define TIMER_IRQ_PRI       APP_TIMER_CONFIG_IRQ_PRIORITY
#define SWI_IRQ_PRI         APP_TIMER_CONFIG_IRQ_PRIORITY

/* 24-bit counter to match original RTC behavior */
#define MAX_COUNTER_VAL     0x00FFFFFF
#define COMPARE_OFFSET_MIN  3
#define MAX_TASKS_DELAY     47

/* SWI for deferred timer list processing */
#if (APP_TIMER_CONFIG_SWI_NUMBER == 0)
#define SWI_IRQn_INST SWI00_IRQn
#elif (APP_TIMER_CONFIG_SWI_NUMBER == 1)
#define SWI_IRQn_INST SWI01_IRQn
#else
#error "Unsupported SWI number."
#endif

#define MODULE_INITIALIZED (m_op_queue.size != 0)

/*--- Timer node (must match APP_TIMER_NODE_SIZE) ---*/
typedef struct {
    uint32_t                    ticks_to_expire;
    uint32_t                    ticks_at_start;
    uint32_t                    ticks_first_interval;
    uint32_t                    ticks_periodic_interval;
    bool                        is_running;
    app_timer_mode_t            mode;
    app_timer_timeout_handler_t p_timeout_handler;
    void *                      p_context;
    void *                      next;
} timer_node_t;

STATIC_ASSERT(sizeof(timer_node_t) == APP_TIMER_NODE_SIZE);

typedef enum {
    TIMER_USER_OP_TYPE_NONE,
    TIMER_USER_OP_TYPE_START,
    TIMER_USER_OP_TYPE_STOP,
    TIMER_USER_OP_TYPE_STOP_ALL
} timer_user_op_type_t;

typedef struct {
    uint32_t ticks_at_start;
    uint32_t ticks_first_interval;
    uint32_t ticks_periodic_interval;
    void *   p_context;
} timer_user_op_start_t;

typedef struct {
    timer_user_op_type_t op_type;
    timer_node_t *       p_node;
    union {
        timer_user_op_start_t start;
    } params;
} timer_user_op_t;

typedef struct {
    uint8_t           first;
    uint8_t           last;
    uint8_t           size;
    timer_user_op_t   user_op_queue[APP_TIMER_CONFIG_OP_QUEUE_SIZE+1];
} timer_op_queue_t;

STATIC_ASSERT(sizeof(timer_op_queue_t) % 4 == 0);

#define CONTEXT_QUEUE_SIZE_MAX      (2)

static timer_op_queue_t              m_op_queue;
static timer_node_t *                mp_timer_id_head;
static uint32_t                      m_ticks_latest;
static uint32_t                      m_ticks_elapsed[CONTEXT_QUEUE_SIZE_MAX];
static uint8_t                       m_ticks_elapsed_q_read_ind;
static uint8_t                       m_ticks_elapsed_q_write_ind;
static bool                          m_timer_running;
static bool                          m_timer_reset;

/*--- TIMER peripheral helpers ---*/

static void timer_init(uint32_t prescaler)
{
    (void)prescaler; /* We use a fixed prescaler for the hardware timer */

    nrf_timer_task_trigger(TIMER_INST, NRF_TIMER_TASK_STOP);
    nrf_timer_task_trigger(TIMER_INST, NRF_TIMER_TASK_CLEAR);

    /* 24-bit mode to match MAX_COUNTER_VAL = 0x00FFFFFF */
    nrf_timer_bit_width_set(TIMER_INST, NRF_TIMER_BIT_WIDTH_24);

    /* Prescaler 12: 128 MHz / 2^12 = 31250 Hz (close to 32768 Hz).
     * nrf_timer_frequency_set() enum assumes 16 MHz base clock, but nRF54L
     * timers run at 128 MHz, so we set the prescaler register directly. */
    TIMER_INST->PRESCALER = 12;

    /* Timer mode (not counter mode) */
    nrf_timer_mode_set(TIMER_INST, NRF_TIMER_MODE_TIMER);

    NVIC_SetPriority(TIMER_IRQn_INST, TIMER_IRQ_PRI);
}

static void timer_start_hw(void)
{
    /* Enable compare[0] interrupt */
    nrf_timer_int_enable(TIMER_INST, nrf_timer_compare_int_get(0));

    NVIC_ClearPendingIRQ(TIMER_IRQn_INST);
    NVIC_EnableIRQ(TIMER_IRQn_INST);

    nrf_timer_task_trigger(TIMER_INST, NRF_TIMER_TASK_START);
    nrf_delay_us(MAX_TASKS_DELAY);

    m_timer_running = true;
}

static void timer_stop_hw(void)
{
    NVIC_DisableIRQ(TIMER_IRQn_INST);

    nrf_timer_int_disable(TIMER_INST, nrf_timer_compare_int_get(0));

    nrf_timer_task_trigger(TIMER_INST, NRF_TIMER_TASK_STOP);
    nrf_delay_us(MAX_TASKS_DELAY);

    nrf_timer_task_trigger(TIMER_INST, NRF_TIMER_TASK_CLEAR);
    m_ticks_latest = 0;
    nrf_delay_us(MAX_TASKS_DELAY);

    m_timer_running = false;
}

static __INLINE uint32_t timer_counter_get(void)
{
    /* Capture current counter value into CC[1] */
    nrf_timer_task_trigger(TIMER_INST, nrf_timer_capture_task_get(1));
    return nrf_timer_cc_get(TIMER_INST, NRF_TIMER_CC_CHANNEL1) & MAX_COUNTER_VAL;
}

static __INLINE uint32_t ticks_diff_get(uint32_t ticks_now, uint32_t ticks_old)
{
    return ((ticks_now - ticks_old) & MAX_COUNTER_VAL);
}

static __INLINE void timer_compare0_set(uint32_t value)
{
    nrf_timer_cc_set(TIMER_INST, NRF_TIMER_CC_CHANNEL0, value & MAX_COUNTER_VAL);
}

/*--- Timer list management (same logic as SDK app_timer.c) ---*/

static void timer_list_insert(timer_node_t * p_timer)
{
    if (mp_timer_id_head == NULL) {
        mp_timer_id_head = p_timer;
    } else {
        if (p_timer->ticks_to_expire <= mp_timer_id_head->ticks_to_expire) {
            mp_timer_id_head->ticks_to_expire -= p_timer->ticks_to_expire;
            p_timer->next = mp_timer_id_head;
            mp_timer_id_head = p_timer;
        } else {
            timer_node_t * p_previous = mp_timer_id_head;
            timer_node_t * p_current = mp_timer_id_head;
            uint32_t ticks_to_expire = p_timer->ticks_to_expire;

            while ((p_current != NULL) && (ticks_to_expire > p_current->ticks_to_expire)) {
                ticks_to_expire -= p_current->ticks_to_expire;
                p_previous = p_current;
                p_current = p_current->next;
            }
            if (p_current != NULL) {
                p_current->ticks_to_expire -= ticks_to_expire;
            }
            p_timer->ticks_to_expire = ticks_to_expire;
            p_timer->next = p_current;
            p_previous->next = p_timer;
        }
    }
}

static bool timer_list_remove(timer_node_t * p_timer)
{
    timer_node_t * p_old_head = mp_timer_id_head;
    timer_node_t * p_previous = mp_timer_id_head;
    timer_node_t * p_current = p_previous;

    while (p_current != NULL) {
        if (p_current == p_timer) break;
        p_previous = p_current;
        p_current = p_current->next;
    }

    if (p_current == NULL) return false;

    if (p_previous == p_current) {
        mp_timer_id_head = mp_timer_id_head->next;
        if (mp_timer_id_head == NULL) {
            nrf_timer_task_trigger(TIMER_INST, NRF_TIMER_TASK_CLEAR);
            m_ticks_latest = 0;
            m_timer_reset = true;
            nrf_delay_us(MAX_TASKS_DELAY);
        }
    }

    uint32_t timeout = p_current->ticks_to_expire;
    p_previous->next = p_current->next;
    p_current = p_previous->next;
    if (p_current != NULL) {
        p_current->ticks_to_expire += timeout;
    }

    return (p_old_head != mp_timer_id_head);
}

static void timer_timeouts_check_sched(void)
{
    NVIC_SetPendingIRQ(TIMER_IRQn_INST);
}

static void timer_list_handler_sched(void)
{
    NVIC_SetPendingIRQ(SWI_IRQn_INST);
}

#if APP_TIMER_CONFIG_USE_SCHEDULER
static void timeout_handler_scheduled_exec(void * p_event_data, uint16_t event_size)
{
    APP_ERROR_CHECK_BOOL(event_size == sizeof(app_timer_event_t));
    app_timer_event_t const * p_timer_event = (app_timer_event_t *)p_event_data;
    p_timer_event->timeout_handler(p_timer_event->p_context);
}
#endif

static void timeout_handler_exec(timer_node_t * p_timer)
{
#if APP_TIMER_CONFIG_USE_SCHEDULER
    app_timer_event_t timer_event;
    timer_event.timeout_handler = p_timer->p_timeout_handler;
    timer_event.p_context = p_timer->p_context;
    uint32_t err_code = app_sched_event_put(&timer_event, sizeof(timer_event), timeout_handler_scheduled_exec);
    APP_ERROR_CHECK(err_code);
#else
    p_timer->p_timeout_handler(p_timer->p_context);
#endif
}

static void timer_timeouts_check(void)
{
    if (mp_timer_id_head != NULL) {
        timer_node_t * p_timer;
        timer_node_t * p_previous_timer;
        uint32_t ticks_elapsed;
        uint32_t ticks_expired = 0;

        ticks_elapsed = ticks_diff_get(timer_counter_get(), m_ticks_latest);
        p_timer = mp_timer_id_head;

        while (p_timer != NULL) {
            if (ticks_elapsed < p_timer->ticks_to_expire) break;
            ticks_elapsed -= p_timer->ticks_to_expire;
            ticks_expired += p_timer->ticks_to_expire;
            p_previous_timer = p_timer;
            p_timer = p_timer->next;
            if (p_previous_timer->is_running) {
                p_previous_timer->is_running = false;
                timeout_handler_exec(p_previous_timer);
            }
        }

        if (m_ticks_elapsed_q_read_ind == m_ticks_elapsed_q_write_ind) {
            if (++m_ticks_elapsed_q_write_ind == CONTEXT_QUEUE_SIZE_MAX) {
                m_ticks_elapsed_q_write_ind = 0;
            }
        }
        m_ticks_elapsed[m_ticks_elapsed_q_write_ind] = ticks_expired;
        timer_list_handler_sched();
    }
}

static bool elapsed_ticks_acquire(uint32_t * p_ticks_elapsed)
{
    if (m_ticks_elapsed_q_read_ind != m_ticks_elapsed_q_write_ind) {
        m_ticks_elapsed_q_read_ind++;
        if (m_ticks_elapsed_q_read_ind == CONTEXT_QUEUE_SIZE_MAX) {
            m_ticks_elapsed_q_read_ind = 0;
        }
        *p_ticks_elapsed = m_ticks_elapsed[m_ticks_elapsed_q_read_ind];
        m_ticks_latest += *p_ticks_elapsed;
        m_ticks_latest &= MAX_COUNTER_VAL;
        return true;
    } else {
        *p_ticks_elapsed = 0;
        return false;
    }
}

static void expired_timers_handler(uint32_t ticks_elapsed, uint32_t ticks_previous,
                                   timer_node_t ** p_restart_list_head)
{
    uint32_t ticks_expired = 0;
    while (mp_timer_id_head != NULL) {
        timer_node_t * p_timer = mp_timer_id_head;
        if (ticks_elapsed < p_timer->ticks_to_expire) {
            p_timer->ticks_to_expire -= ticks_elapsed;
            break;
        }
        ticks_elapsed -= p_timer->ticks_to_expire;
        ticks_expired += p_timer->ticks_to_expire;
        p_timer->ticks_to_expire = 0;

        timer_node_t * p_timer_expired = mp_timer_id_head;
        mp_timer_id_head = p_timer->next;

        if (p_timer->ticks_periodic_interval != 0) {
            p_timer->ticks_at_start = (ticks_previous + ticks_expired) & MAX_COUNTER_VAL;
            p_timer->ticks_first_interval = p_timer->ticks_periodic_interval;
            p_timer->next = *p_restart_list_head;
            *p_restart_list_head = p_timer_expired;
        }
    }
}

static bool list_insertions_handler(timer_node_t * p_restart_list_head)
{
    bool compare_update = false;
    timer_node_t * p_timer_id_old_head = mp_timer_id_head;

    while ((p_restart_list_head != NULL) || (m_op_queue.first != m_op_queue.last)) {
        timer_node_t * p_timer;

        if (p_restart_list_head != NULL) {
            p_timer = p_restart_list_head;
            p_restart_list_head = p_timer->next;
        } else {
            timer_user_op_t * p_user_op = &m_op_queue.user_op_queue[m_op_queue.first];
            m_op_queue.first++;
            if (m_op_queue.first == m_op_queue.size) m_op_queue.first = 0;
            p_timer = p_user_op->p_node;

            switch (p_user_op->op_type) {
                case TIMER_USER_OP_TYPE_STOP:
                    if (timer_list_remove(p_user_op->p_node)) compare_update = true;
                    p_timer->is_running = false;
                    continue;
                case TIMER_USER_OP_TYPE_STOP_ALL:
                    while (mp_timer_id_head != NULL) {
                        timer_node_t * p_head = mp_timer_id_head;
                        p_head->is_running = false;
                        mp_timer_id_head = p_head->next;
                    }
                    continue;
                case TIMER_USER_OP_TYPE_START:
                    break;
                default:
                    continue;
            }

            if (p_timer->is_running) continue;

            p_timer->ticks_at_start          = p_user_op->params.start.ticks_at_start;
            p_timer->ticks_first_interval    = p_user_op->params.start.ticks_first_interval;
            p_timer->ticks_periodic_interval = p_user_op->params.start.ticks_periodic_interval;
            p_timer->p_context               = p_user_op->params.start.p_context;

            if (m_timer_reset) p_timer->ticks_at_start = 0;
        }

        if (((p_timer->ticks_at_start - m_ticks_latest) & MAX_COUNTER_VAL) < (MAX_COUNTER_VAL / 2)) {
            p_timer->ticks_to_expire = ticks_diff_get(p_timer->ticks_at_start, m_ticks_latest) +
                                       p_timer->ticks_first_interval;
        } else {
            uint32_t delta = ticks_diff_get(m_ticks_latest, p_timer->ticks_at_start);
            p_timer->ticks_to_expire = (p_timer->ticks_first_interval > delta) ?
                                       (p_timer->ticks_first_interval - delta) : 0;
        }

        p_timer->ticks_at_start = 0;
        p_timer->ticks_first_interval = 0;
        p_timer->is_running = true;
        p_timer->next = NULL;
        timer_list_insert(p_timer);
    }

    return (compare_update || (mp_timer_id_head != p_timer_id_old_head));
}

static void compare_reg_update(timer_node_t * p_timer_id_head_old)
{
    if (mp_timer_id_head != NULL) {
        uint32_t ticks_to_expire = mp_timer_id_head->ticks_to_expire;
        uint32_t pre_counter_val = timer_counter_get();
        uint32_t cc = m_ticks_latest;
        uint32_t ticks_elapsed = ticks_diff_get(pre_counter_val, cc) + COMPARE_OFFSET_MIN;

        if (!m_timer_running) {
            timer_start_hw();
        }

        cc += (ticks_elapsed < ticks_to_expire) ? ticks_to_expire : ticks_elapsed;
        cc &= MAX_COUNTER_VAL;
        timer_compare0_set(cc);

        uint32_t post_counter_val = timer_counter_get();
        if ((ticks_diff_get(post_counter_val, pre_counter_val) + COMPARE_OFFSET_MIN) >
            ticks_diff_get(cc, pre_counter_val)) {
            timer_compare0_set(timer_counter_get());
            nrf_delay_us(MAX_TASKS_DELAY);
            timer_timeouts_check_sched();
        }
    } else {
        timer_stop_hw();
    }
}

static void timer_list_handler(void)
{
    timer_node_t * p_restart_list_head = NULL;
    uint32_t ticks_elapsed;
    uint32_t ticks_previous;
    bool ticks_have_elapsed;
    bool compare_update = false;
    timer_node_t * p_timer_id_head_old;

    ticks_previous = m_ticks_latest;
    p_timer_id_head_old = mp_timer_id_head;
    ticks_have_elapsed = elapsed_ticks_acquire(&ticks_elapsed);

    if (ticks_have_elapsed) {
        expired_timers_handler(ticks_elapsed, ticks_previous, &p_restart_list_head);
        compare_update = true;
    }

    if (list_insertions_handler(p_restart_list_head)) {
        compare_update = true;
    }

    if (compare_update) {
        compare_reg_update(p_timer_id_head_old);
    }
    m_timer_reset = false;
}

/*--- Op queue ---*/

static timer_user_op_t * user_op_alloc(uint8_t * p_last_index)
{
    uint8_t last = m_op_queue.last + 1;
    if (last == m_op_queue.size) last = 0;
    if (last == m_op_queue.first) return NULL;
    *p_last_index = last;
    return &m_op_queue.user_op_queue[m_op_queue.last];
}

static uint32_t timer_start_op_schedule(timer_node_t * p_node, uint32_t timeout_initial,
                                         uint32_t timeout_periodic, void * p_context)
{
    uint8_t last_index;
    uint32_t err_code = NRF_SUCCESS;

    CRITICAL_REGION_ENTER();
    timer_user_op_t * p_user_op = user_op_alloc(&last_index);
    if (p_user_op == NULL) {
        err_code = NRF_ERROR_NO_MEM;
    } else {
        p_user_op->op_type = TIMER_USER_OP_TYPE_START;
        p_user_op->p_node = p_node;
        p_user_op->params.start.ticks_at_start = timer_counter_get();
        p_user_op->params.start.ticks_first_interval = timeout_initial;
        p_user_op->params.start.ticks_periodic_interval = timeout_periodic;
        p_user_op->params.start.p_context = p_context;
        m_op_queue.last = last_index;
    }
    CRITICAL_REGION_EXIT();

    if (err_code == NRF_SUCCESS) timer_list_handler_sched();
    return err_code;
}

static uint32_t timer_stop_op_schedule(timer_node_t * p_node, timer_user_op_type_t op_type)
{
    uint8_t last_index;
    uint32_t err_code = NRF_SUCCESS;

    CRITICAL_REGION_ENTER();
    timer_user_op_t * p_user_op = user_op_alloc(&last_index);
    if (p_user_op == NULL) {
        err_code = NRF_ERROR_NO_MEM;
    } else {
        p_user_op->op_type = op_type;
        p_user_op->p_node = p_node;
        m_op_queue.last = last_index;
    }
    CRITICAL_REGION_EXIT();

    if (err_code == NRF_SUCCESS) timer_list_handler_sched();
    return err_code;
}

/*--- IRQ Handlers ---*/

void TIMER20_IRQHandler(void)
{
    /* Clear compare events */
    nrf_timer_event_clear(TIMER_INST, NRF_TIMER_EVENT_COMPARE0);
    nrf_timer_event_clear(TIMER_INST, NRF_TIMER_EVENT_COMPARE1);
    nrf_timer_event_clear(TIMER_INST, NRF_TIMER_EVENT_COMPARE2);
    nrf_timer_event_clear(TIMER_INST, NRF_TIMER_EVENT_COMPARE3);

    timer_timeouts_check();
}

void SWI00_IRQHandler(void)
{
    timer_list_handler();
}

/*--- Public API ---*/

ret_code_t app_timer_init(void)
{
    timer_stop_hw();

    m_op_queue.first = 0;
    m_op_queue.last = 0;
    m_op_queue.size = APP_TIMER_CONFIG_OP_QUEUE_SIZE + 1;

    mp_timer_id_head = NULL;
    m_ticks_elapsed_q_read_ind = 0;
    m_ticks_elapsed_q_write_ind = 0;

    NVIC_ClearPendingIRQ(SWI_IRQn_INST);
    NVIC_SetPriority(SWI_IRQn_INST, SWI_IRQ_PRI);
    NVIC_EnableIRQ(SWI_IRQn_INST);

    timer_init(APP_TIMER_CONFIG_RTC_FREQUENCY);
    m_ticks_latest = timer_counter_get();

    return NRF_SUCCESS;
}

ret_code_t app_timer_create(app_timer_id_t const * p_timer_id, app_timer_mode_t mode,
                            app_timer_timeout_handler_t timeout_handler)
{
    VERIFY_MODULE_INITIALIZED();
    if (timeout_handler == NULL) return NRF_ERROR_INVALID_PARAM;
    if (p_timer_id == NULL) return NRF_ERROR_INVALID_PARAM;
    if (((timer_node_t*)*p_timer_id)->is_running) return NRF_ERROR_INVALID_STATE;

    timer_node_t * p_node = (timer_node_t *)*p_timer_id;
    p_node->is_running = false;
    p_node->mode = mode;
    p_node->p_timeout_handler = timeout_handler;
    return NRF_SUCCESS;
}

ret_code_t app_timer_start(app_timer_id_t timer_id, uint32_t timeout_ticks, void * p_context)
{
    timer_node_t * p_node = (timer_node_t*)timer_id;
    VERIFY_MODULE_INITIALIZED();
    if (timer_id == 0) return NRF_ERROR_INVALID_STATE;
    if (timeout_ticks < APP_TIMER_MIN_TIMEOUT_TICKS) return NRF_ERROR_INVALID_PARAM;
    if (p_node->p_timeout_handler == NULL) return NRF_ERROR_INVALID_STATE;

    uint32_t timeout_periodic = (p_node->mode == APP_TIMER_MODE_REPEATED) ? timeout_ticks : 0;
    return timer_start_op_schedule(p_node, timeout_ticks, timeout_periodic, p_context);
}

ret_code_t app_timer_stop(app_timer_id_t timer_id)
{
    timer_node_t * p_node = (timer_node_t*)timer_id;
    VERIFY_MODULE_INITIALIZED();
    if ((timer_id == NULL) || (p_node->p_timeout_handler == NULL)) return NRF_ERROR_INVALID_STATE;
    p_node->is_running = false;
    return timer_stop_op_schedule(p_node, TIMER_USER_OP_TYPE_STOP);
}

ret_code_t app_timer_stop_all(void)
{
    VERIFY_MODULE_INITIALIZED();
    return timer_stop_op_schedule(NULL, TIMER_USER_OP_TYPE_STOP_ALL);
}

uint32_t app_timer_cnt_get(void)
{
    return timer_counter_get();
}

uint32_t app_timer_cnt_diff_compute(uint32_t ticks_to, uint32_t ticks_from)
{
    return ticks_diff_get(ticks_to, ticks_from);
}

void app_timer_pause(void)
{
    nrf_timer_task_trigger(TIMER_INST, NRF_TIMER_TASK_STOP);
}

void app_timer_resume(void)
{
    nrf_timer_task_trigger(TIMER_INST, NRF_TIMER_TASK_START);
}

#endif //NRF_MODULE_ENABLED(APP_TIMER)
