/* Money scale (fork of fintan-ohea; docs/specs/2026-10-24-money-scale-spec.md).
 *
 * The original shows every amount through its currency: Cash::get_display_amount (0x43f460) and the steps
 * (0x43f4e0, 0x43f580) multiply by the exchange rate g_currency_exchange_rate (double 0xad9cb8), and the input
 * (Cash::set_from_display 0x43f4c0) divides by it; the presets (0x43f620, 0x43f720) multiply by
 * g_currency_preset_amount_scale (double 0xad9da0) and divide by the rate. cash_set_currency (0x43f7f0, called by
 * Config::Config 0x5d76ce and Config::apply_and_save 0x5d85e0) sets both; nothing else writes them.
 *
 * With money_scale = 5, 7 or 10 the wrap of cash_set_currency multiplies both by the factor, so the player sees
 * and enters modern amounts while the game keeps its own. money_apply sets them again when the option changes in
 * the options box.
 *
 * Same game (Q152): five functions of the game's own logic go through the rate. They compare amounts rounded as
 * shown (Cash::get_display_amount with 4: TRANSFER_OFFER::has_same_terms 0x8e7f70 at 0x8e7fb1..0x8e8083,
 * transfer_manager_selling_club_must_accept 0x8cd240 at 0x8cd31a) or step an amount by a step rounded as shown
 * (Cash::step_up: TRANSFER_MANAGER::check_offer_restrictions 0x8cfc90 at 0x8cfe51, STAFF_CONTRACT::
 * increase_wage_step 0x867b70 at 0x867bb8..0x867c2e; decrease_wage_step 0x867c50). Their wraps run them with the
 * game's own rates, so they decide as the original does. */
#include "win.h"
#include "options.h"

#include <stdio.h>
#include <stdlib.h>

#define RATE 0xad9cb8u              /* double: pounds to the shown currency */
#define PRESET_SCALE 0xad9da0u      /* double: the preset amounts in the shown currency */

static const int factors[] = {1, 5, 7, 10};
static double base_rate, base_preset;
static int have_base;

int money_factor(void)
{
    int i = options_enum(OPT_MONEY_SCALE);
    return i >= 0 && i < (int)(sizeof factors / sizeof factors[0]) ? factors[i] : 1;
}

static int unscaled;                /* the depth of the logic functions that run with the game's own rates */

static void set_rates(int f)
{
    wrf64(g_mem, RATE, base_rate * f);
    wrf64(g_mem, PRESET_SCALE, base_preset * f);
}

void money_apply(void)
{
    if (!have_base || unscaled) return;
    int f = money_factor();
    set_rates(f);
    fprintf(stderr, "recomp: money scale %d: rate %g, presets %g\n", f, base_rate * f, base_preset * f);
}

/* An amount of the game as the player sees it: times the rate, cut as the game cuts it (0x945640). */
int64_t money_shown(int64_t v)
{
    return (int64_t)((double)v * rdf64(g_mem, RATE));
}

/* An amount the player sees as the game keeps it. */
int64_t money_internal(int64_t shown)
{
    double r = rdf64(g_mem, RATE);
    return r > 0 ? (int64_t)((double)shown / r) : shown;
}

void o_0043f7f0(void);

/* cash_set_currency, cdecl (currency): the game's rates, then the factor. */
RT_WRAP(0043f7f0, set_currency)
{
    o_0043f7f0();
    if (rt_unwinding) return;
    base_rate = rdf64(g_mem, RATE);
    base_preset = rdf64(g_mem, PRESET_SCALE);
    have_base = 1;
    if (money_factor() != 1) money_apply();
}

/* The game's own rates around a function of its logic; 1 when they were changed. RECOMP_MONEY_LOG (test only)
 * writes each call. */
static int unscale(const char *name)
{
    static int log = -1;
    if (log < 0) log = getenv("RECOMP_MONEY_LOG") != NULL;
    if (log) fprintf(stderr, "recomp: money logic %s\n", name);
    int changed = have_base && !unscaled && money_factor() != 1;
    unscaled++;
    if (changed) set_rates(1);
    return changed;
}

static void rescale(int changed)
{
    unscaled--;
    if (changed) set_rates(money_factor());
}

void o_008e7f70(void);

RT_WRAP(008e7f70, has_same_terms)
{
    int changed = unscale("has_same_terms");
    o_008e7f70();
    rescale(changed);
}

void o_008cd240(void);

RT_WRAP(008cd240, selling_club_must_accept)
{
    int changed = unscale("selling_club_must_accept");
    o_008cd240();
    rescale(changed);
}

void o_008cfc90(void);

RT_WRAP(008cfc90, check_offer_restrictions)
{
    int changed = unscale("check_offer_restrictions");
    o_008cfc90();
    rescale(changed);
}

void o_00867b70(void);

RT_WRAP(00867b70, increase_wage_step)
{
    int changed = unscale("increase_wage_step");
    o_00867b70();
    rescale(changed);
}

void o_00867c50(void);

RT_WRAP(00867c50, decrease_wage_step)
{
    int changed = unscale("decrease_wage_step");
    o_00867c50();
    rescale(changed);
}
