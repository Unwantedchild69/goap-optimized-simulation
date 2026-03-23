#include <bits/stdc++.h>
#include <omp.h>      
#include <algorithm>  
#include <random>    
#include <chrono>
#include <atomic>
#include "raylib.h"
#include "ankerl/unordered_dense.h"

using namespace std;

Color TEAL = { 0, 128, 128, 255 };
std::atomic<int> central_bank_money{100000000}; 
std::atomic<int> global_bridge_wood{0};
std::atomic<int> global_houses{5}; 
std::atomic<int> m_wood{0}, m_ore{0}, m_wheat{0}, m_tools{0}, m_bread{0}, m_beer{0};
const int MAX_BRIDGE = 100000; 

class FastRandom 
{
private:
    uint64_t s[2];

    static inline uint64_t rotl(const uint64_t x, int k) 
    {
        return (x << k) | (x >> (64 - k));
    }

public:
    FastRandom(uint64_t seed1, uint64_t seed2) 
    {
        s[0] = seed1;
        s[1] = seed2;
    }

    inline uint64_t next() 
    {
        const uint64_t s0 = s[0];
        uint64_t s1 = s[1];
        const uint64_t result = s0 + s1;

        s1 ^= s0;
        s[0] = rotl(s0, 24) ^ s1 ^ (s1 << 16);
        s[1] = rotl(s1, 37);

        return result;
    }
};

inline FastRandom& get_tls_rng() 
{

    thread_local FastRandom rng
    (
        std::random_device{}(), 
        std::random_device{}()
    );
    return rng;
}

int get_random(int min, int max) 
{
    
    uint32_t range = (uint32_t)(max - min + 1);
    
    uint32_t rnd = get_tls_rng().next() % range;
    
    return min + (int)rnd;
}

struct Location 
{ 
    Rectangle r; 
    Color c; 
    const char* n; 
    Vector2 getPoint() 
    { 
    return 
        { 
        (float)get_random((int)r.x+5, (int)(r.x+r.width-5)), 
        (float)get_random((int)r.y+5, (int)(r.y+r.height-5)) 
        }; 
    }
};

Location river = {{50,0,100,900}, BLUE, "River"}; Location bridge = {{150,400,100,100}, BLANK, ""};
Location forest = {{300, 50, 200, 150}, DARKGREEN, "Forest"}; Location mine = {{300, 250, 200, 150}, GRAY, "Mine"};
Location farm = {{300, 450, 200, 150}, YELLOW, "Farm"}; Location inn = {{300, 650, 200, 150}, DARKBLUE, "Inn"};

Location market = {{700, 50, 400, 150}, ORANGE, "MARKET"}; Location smith = {{1150, 50, 200, 150}, BLACK, "Smith"}; 
Location bakery = {{700, 250, 200, 150}, PINK, "Bakery"}; Location well = {{950, 250, 200, 150}, SKYBLUE, "Well"};
Location tavern = {{1200, 250, 150, 150}, MAROON, "Tavern"}; 
Location suburbs = {{700, 450, 650, 150}, PURPLE, "Suburbs (Houses)"}; 
Location townhall = {{700, 650, 300, 150}, GOLD, "Townhall"}; Location library = {{1050, 650, 300, 150}, TEAL, "Library"};

enum goal_type : uint8_t 
{ 
    FIX_HUNGER, FIX_TIREDNESS, FIX_THIRST, CONTRIBUTE_BRIDGE, GET_RICH, BECOME_HOMEOWNER, BECOME_EDUCATED, PAY_TAXES 
};

enum action_type : uint8_t 
{ 
    CHOP_WOOD_BARE, CHOP_WOOD_TOOL, MINE_ORE_BARE, MINE_ORE_TOOL, FARM_WHEAT, CRAFT_TOOL, BAKE_BREAD, BUILD_BRIDGE,
    SELL_WOOD, SELL_ORE, SELL_WHEAT, SELL_TOOL, SELL_BREAD, BUY_WOOD, BUY_ORE, BUY_WHEAT, BUY_TOOL, BUY_BREAD,
    EAT_BREAD, RENT_BED, SLEEP, DRINK_WATER, BUY_HOUSE, RELAX_HOUSE,
    BREW_BEER, SELL_BEER, BUY_BEER, DRINK_BEER, BUILD_HOUSE_ACT, PAY_TAX_ACT, WORK_GUARD, STUDY
};

struct AgentSkills 
{ 
    uint8_t lumberjack, miner, farmer, baker, builder, smith, combat; 
};

struct world_state 
{

    int money;
    int32_t g_money;
    int16_t target_money;
    int16_t g_wood, g_ore, g_wheat, g_tools, g_bread, g_houses, g_bridge_wood, g_beer;
    uint8_t inv_wood, inv_ore, inv_wheat, inv_bread, inv_tool, inv_beer, combat_skill;
    bool has_tool:1;
    bool is_hungry:1;
    bool is_tired:1;
    bool is_thirsty:1; 
    bool has_bed:1;
    bool built_bridge:1; 
    bool owns_house:1;
    bool is_educated:1;
    bool paid_tax:1;

};

int get_dynamic_cost(action_type act, int base_cost, const AgentSkills& skills) 
{
    int skill_level = 0;
    if(act == CHOP_WOOD_BARE || act == CHOP_WOOD_TOOL) skill_level = skills.lumberjack;
    else if(act == MINE_ORE_BARE || act == MINE_ORE_TOOL) skill_level = skills.miner;
    else if(act == FARM_WHEAT || act == BREW_BEER) skill_level = skills.farmer;
    else if(act == BAKE_BREAD) skill_level = skills.baker;
    else if(act == BUILD_BRIDGE || act == BUILD_HOUSE_ACT) skill_level = skills.builder;
    else if(act == CRAFT_TOOL) skill_level = skills.smith;
    else return base_cost;
    return max(1, base_cost + (10 - skill_level));
}

bool pre_chop_bare(const world_state& s) { return !s.has_tool && s.inv_wood < 1; } world_state eff_chop_bare(const world_state& s) { world_state ns = s; ns.inv_wood++; return ns; } 
bool pre_chop_tool(const world_state& s) { return s.has_tool && s.inv_wood < 2; } world_state eff_chop_tool(const world_state& s) { world_state ns = s; ns.inv_wood+=2; return ns; } 
bool pre_mine_bare(const world_state& s) { return !s.has_tool && s.inv_ore < 1; } world_state eff_mine_bare(const world_state& s) { world_state ns = s; ns.inv_ore++; return ns; } 
bool pre_mine_tool(const world_state& s) { return s.has_tool && s.inv_ore < 2; } world_state eff_mine_tool(const world_state& s) { world_state ns = s; ns.inv_ore+=2; return ns; } 
bool pre_farm(const world_state& s) { return s.inv_wheat < 1; } world_state eff_farm(const world_state& s) { world_state ns = s; ns.inv_wheat++; return ns; } 
bool pre_craft(const world_state& s) { return s.inv_ore >= 1 && s.inv_tool < 1; } world_state eff_craft(const world_state& s) { world_state ns = s; ns.inv_ore--; ns.inv_tool++; ns.has_tool = true; return ns; } 
bool pre_bake(const world_state& s) { return s.inv_wheat >= 1 && s.inv_bread < 1; } world_state eff_bake(const world_state& s) { world_state ns = s; ns.inv_wheat--; ns.inv_bread++; return ns; } 
bool pre_sell_wood(const world_state& s) { return s.inv_wood > 0 && s.g_money >= 2 && s.g_wood < 100000; } world_state eff_sell_wood(const world_state& s) { world_state ns=s; ns.inv_wood--; ns.money+=2; ns.g_money-=2; ns.g_wood++; return ns;} 
bool pre_buy_wood(const world_state& s) { return s.money >= 3 && s.g_wood > 0; } world_state eff_buy_wood(const world_state& s) { world_state ns=s; ns.money-=3; ns.g_money+=3; ns.g_wood--; ns.inv_wood++; return ns;} 
bool pre_sell_ore(const world_state& s) { return s.inv_ore > 0 && s.g_money >= 3 && s.g_ore < 50000; } world_state eff_sell_ore(const world_state& s) { world_state ns=s; ns.inv_ore--; ns.money+=3; ns.g_money-=3; ns.g_ore++; return ns;} 
bool pre_buy_ore(const world_state& s) { return s.money >= 4 && s.g_ore > 0; } world_state eff_buy_ore(const world_state& s) { world_state ns=s; ns.money-=4; ns.g_money+=4; ns.g_ore--; ns.inv_ore++; return ns;} 
bool pre_sell_wheat(const world_state& s) { return s.inv_wheat > 0 && s.g_money >= 1 && s.g_wheat < 50000; } world_state eff_sell_wheat(const world_state& s) { world_state ns=s; ns.inv_wheat--; ns.money+=1; ns.g_money-=1; ns.g_wheat++; return ns;} 
bool pre_buy_wheat(const world_state& s) { return s.money >= 2 && s.g_wheat > 0; } world_state eff_buy_wheat(const world_state& s) { world_state ns=s; ns.money-=2; ns.g_money+=2; ns.g_wheat--; ns.inv_wheat++; return ns;} 
bool pre_sell_bread(const world_state& s) { return s.inv_bread > 0 && s.g_money >= 3 && s.g_bread < 50000; } world_state eff_sell_bread(const world_state& s) { world_state ns=s; ns.inv_bread--; ns.money+=3; ns.g_money-=3; ns.g_bread++; return ns;} 
bool pre_buy_bread(const world_state& s) { return s.money >= 4 && s.g_bread > 0; } world_state eff_buy_bread(const world_state& s) { world_state ns=s; ns.money-=4; ns.g_money+=4; ns.g_bread--; ns.inv_bread++; return ns;} 
bool pre_sell_tool(const world_state& s) { return s.inv_tool > 0 && s.g_money >= 8 && s.g_tools < 20000; } world_state eff_sell_tool(const world_state& s) { world_state ns=s; ns.inv_tool--; ns.money+=8; ns.g_money-=8; ns.g_tools++; ns.has_tool=false; return ns;} 
bool pre_buy_tool(const world_state& s) { return s.money >= 10 && !s.has_tool && s.g_tools > 0; } world_state eff_buy_tool(const world_state& s) { world_state ns=s; ns.money-=10; ns.g_money+=10; ns.g_tools--; ns.has_tool=true; return ns;} 
bool pre_eat(const world_state& s) { return s.inv_bread > 0 && s.is_hungry; } world_state eff_eat(const world_state& s) { world_state ns=s; ns.inv_bread--; ns.is_hungry=false; return ns; } 
bool pre_rent(const world_state& s) { return s.money >= 2 && !s.has_bed && !s.owns_house; } world_state eff_rent(const world_state& s) { world_state ns=s; ns.money-=2; ns.g_money+=2; ns.has_bed=true; return ns; } 
bool pre_sleep(const world_state& s) { return s.has_bed && s.is_tired; } world_state eff_sleep(const world_state& s) { world_state ns=s; ns.has_bed=false; ns.is_tired=false; return ns; } 
bool pre_drink(const world_state& s) { return s.is_thirsty && !s.owns_house && !s.inv_beer; } world_state eff_drink(const world_state& s) { world_state ns=s; ns.is_thirsty=false; return ns; } 
bool pre_bridge(const world_state& s) { return s.inv_wood >= 1 && s.g_money >= 10 && s.g_bridge_wood < MAX_BRIDGE; } world_state eff_bridge(const world_state& s) { world_state ns=s; ns.inv_wood--; ns.built_bridge=true; ns.money+=10; ns.g_money-=10; ns.g_bridge_wood++; return ns; } 
bool pre_buy_house(const world_state& s) { return s.money >= 50 && s.g_houses > 0 && !s.owns_house; } world_state eff_buy_house(const world_state& s) { world_state ns=s; ns.money-=50; ns.g_money+=50; ns.g_houses--; ns.owns_house=true; return ns; } 
bool pre_relax(const world_state& s) { return s.owns_house && (s.is_tired || s.is_thirsty); } world_state eff_relax(const world_state& s) { world_state ns=s; ns.is_tired=false; ns.is_thirsty=false; return ns; } 
bool pre_brew(const world_state& s) { return s.inv_wheat >= 1 && s.inv_beer < 1; } world_state eff_brew(const world_state& s) { world_state ns=s; ns.inv_wheat-=1; ns.inv_beer++; return ns; }
bool pre_sell_beer(const world_state& s) { return s.inv_beer > 0 && s.g_money >= 5 && s.g_beer < 20000; } world_state eff_sell_beer(const world_state& s) { world_state ns=s; ns.inv_beer--; ns.money+=5; ns.g_money-=5; ns.g_beer++; return ns; }
bool pre_buy_beer(const world_state& s) { return s.money >= 4 && s.g_beer > 0 && s.inv_beer < 1; } world_state eff_buy_beer(const world_state& s) { world_state ns=s; ns.money-=6; ns.g_money+=6; ns.g_beer--; ns.inv_beer++; return ns; }
bool pre_drink_beer(const world_state& s) { return s.inv_beer > 0 && (s.is_thirsty || s.is_tired); } world_state eff_drink_beer(const world_state& s) { world_state ns=s; ns.inv_beer--; ns.is_thirsty=false; ns.is_tired=false; return ns; }
bool pre_build_h_act(const world_state& s) { return s.inv_wood >= 2 && s.inv_ore >= 2 && s.g_money >= 20; } world_state eff_build_h_act(const world_state& s) { world_state ns=s; ns.inv_wood-=2; ns.inv_ore-=2; ns.money+=20; ns.g_money-=20; ns.g_houses++; return ns; }
bool pre_pay_tax(const world_state& s) { return s.money >= 20 && !s.paid_tax; } world_state eff_pay_tax(const world_state& s) { world_state ns=s; ns.money-=10; ns.g_money+=10; ns.paid_tax=true; return ns; }
bool pre_work_guard(const world_state& s) { return s.g_money >= 5 && s.combat_skill >= 5 && s.is_educated ;} world_state eff_work_guard(const world_state& s) { world_state ns=s; ns.money+=5; ns.g_money-=5; return ns; }
bool pre_study(const world_state& s) { return s.money >= 15 && !s.is_educated; } world_state eff_study(const world_state& s) { world_state ns=s; ns.money-=15; ns.g_money+=15; ns.is_educated=true; return ns; }

struct Action 
{ 
    action_type type; 
    int cost; 
    bool (*pre)(const world_state&); 
    world_state (*eff)(const world_state&); 
};

struct AgentRenderData 
{ 
    Vector2 pos; 
    Color c; 
    float spd; 
    float vx,vy;
    int16_t steps;
};

struct alignas(64) AgentLogicData 
{ 
    world_state state; 
    AgentSkills skills; 
    int16_t action_timer;
    goal_type goal; 
    bool needs_plan; 
};
struct alignas(64) AgentPlan 
{ 
    action_type actions[20]; 
    uint8_t size; 
    uint8_t current_step; 
};
struct Node 
{
    world_state state;
    uint16_t g_cost;
    uint16_t f_cost; 
    action_type plan[20];
    uint8_t plan_size;
    bool operator>(const Node& o) const { return f_cost > o.f_cost; } 
};

uint16_t get_h_cost(const world_state& s, goal_type g) 
{
    if(g == FIX_HUNGER && s.is_hungry) 
        return (s.inv_bread > 0) ? 1 : 5; 
    if(g == FIX_TIREDNESS && s.is_tired) 
        return (s.has_bed || s.owns_house) ? 1 : 4;
    if(g == GET_RICH) return (s.target_money > s.money) ? (s.target_money - s.money) / 3 : 0;
    if(g == BECOME_HOMEOWNER && !s.owns_house) 
        return (s.money >= 50) ? 5 : 20;
    return 0;
}

bool is_goal_met(const world_state& s, goal_type g) 
{
    if(g==FIX_HUNGER) return !s.is_hungry; 
    if(g==FIX_TIREDNESS) return !s.is_tired; 
    if(g==FIX_THIRST) return !s.is_thirsty; 
    if(g==CONTRIBUTE_BRIDGE) return s.built_bridge;
    if(g==GET_RICH) return s.money >= s.target_money; 
    if(g==BECOME_HOMEOWNER) return s.owns_house;
    if(g==BECOME_EDUCATED) return s.is_educated;
    if(g==PAY_TAXES) return s.paid_tax;
    return false;
}

#pragma pack(push, 1) 
struct CompactPlanKey 
{
    uint16_t money;
    uint16_t target_money;
    uint8_t inv_wood, inv_ore, inv_wheat, inv_bread, inv_tool, inv_beer;
    uint8_t goal;
    uint8_t skill_lm, skill_fb, skill_bs, skill_c;  
    uint32_t flags;

    bool operator==(const CompactPlanKey& o) const {
        return std::memcmp(this, &o, sizeof(CompactPlanKey)) == 0;
    }
};
#pragma pack(pop)

struct CompactPlanKeyHasher 
{
    size_t operator()(const CompactPlanKey& k) const 
    {
        return std::hash<std::string_view>()(std::string_view(reinterpret_cast<const char*>(&k), sizeof(CompactPlanKey)));
    }
};

static thread_local ankerl::unordered_dense::map<CompactPlanKey, AgentPlan, CompactPlanKeyHasher> cache_new;
static thread_local ankerl::unordered_dense::map<CompactPlanKey, AgentPlan, CompactPlanKeyHasher> cache_old;

AgentPlan* get_plan(const CompactPlanKey& key) 
{
    auto it_new = cache_new.find(key);
    if (it_new != cache_new.end()) 
        return &(it_new->second);

    auto it_old = cache_old.find(key);
    if (it_old != cache_old.end()) 
    {
        auto res = cache_new.insert({key, it_old->second});
        return &(res.first->second);
    }
    return nullptr; 
}

void save_plan(const CompactPlanKey& key, const AgentPlan& plan) 
{
    cache_new[key] = plan; 

    if (cache_new.size() > 50000) 
    {
        cache_old.clear(); 
        std::swap(cache_new, cache_old); 
    }
}

AgentPlan build_plan(world_state start, goal_type goal, const vector<Action>& actions, const AgentSkills& skills) 
{

    CompactPlanKey key;
    std::memset(&key, 0, sizeof(CompactPlanKey));

    key.money = (uint16_t)min(start.money, 60); 
    if (goal == GET_RICH && start.target_money > start.money) 
    {
        key.target_money = (uint16_t)min((int)(start.target_money - start.money), 60);
    } 
    else 
    {
        key.target_money = 0;
    }
    key.inv_wood = (uint8_t)min((int)start.inv_wood, 10);
    key.inv_ore = (uint8_t)min((int)start.inv_ore, 10);
    key.inv_wheat = (uint8_t)min((int)start.inv_wheat, 10);
    key.inv_bread = (uint8_t)min((int)start.inv_bread, 10);
    key.inv_tool = (uint8_t)min((int)start.inv_tool, 10);
    key.inv_beer = (uint8_t)min((int)start.inv_beer, 10);

    key.goal = goal;

    key.skill_lm = (skills.lumberjack << 4) | skills.miner;
    key.skill_fb = (skills.farmer << 4) | skills.baker;
    key.skill_bs = (skills.builder << 4) | skills.smith;
    key.skill_c = skills.combat;

    uint32_t f = 0;
    f |= (start.has_tool << 0);
    f |= (start.is_hungry << 1);
    f |= (start.is_tired << 2);
    f |= (start.is_thirsty << 3);
    f |= (start.has_bed << 4);
    f |= (start.built_bridge << 5);
    f |= (start.owns_house << 6);
    f |= (start.paid_tax << 7);
    f |= (start.is_educated << 8);

    f |= ((start.g_money >= 10) << 9);
    f |= ((start.g_wood > 0) << 10);
    f |= ((start.g_wood < 100000) << 11);
    f |= ((start.g_ore > 0) << 12);
    f |= ((start.g_ore < 50000) << 13);
    f |= ((start.g_wheat > 0) << 14);
    f |= ((start.g_wheat < 50000) << 15);
    f |= ((start.g_bread > 0) << 16);
    f |= ((start.g_bread < 50000) << 17);
    f |= ((start.g_tools > 0) << 18);
    f |= ((start.g_tools < 20000) << 19);
    f |= ((start.g_beer > 0) << 20);
    f |= ((start.g_beer < 20000) << 21);
    f |= ((start.g_houses > 0) << 22);
    key.flags = f;

    AgentPlan* cached_plan = get_plan(key);
    if (cached_plan != nullptr) 
    {
        return *cached_plan; 
    }

    AgentPlan final_plan; final_plan.size = 0; final_plan.current_step = 0;
    thread_local vector<Node> open_list; open_list.clear(); 
    
    Node start_node; 
    start_node.state = start; 
    start_node.g_cost = 0; 
    start_node.f_cost = get_h_cost(start, goal); 
    start_node.plan_size = 0;
    
    open_list.push_back(start_node);

    int iter = 1500; 
    while(!open_list.empty() && iter-- > 0) 
    {
        pop_heap(open_list.begin(), open_list.end(), greater<Node>());
        Node curr = open_list.back(); open_list.pop_back();

        if(is_goal_met(curr.state, goal)) 
        {
            for(int i=0; i<curr.plan_size; i++) final_plan.actions[i] = curr.plan[i];
            final_plan.size = curr.plan_size; 
            
            save_plan(key, final_plan);
            
            return final_plan;
        }

        for(const auto& a : actions) 
        {
            if(a.pre(curr.state) && curr.plan_size < 20) 
            {
                Node next = curr; 
                next.state = a.eff(curr.state); 
                next.g_cost += get_dynamic_cost(a.type, a.cost, skills); 
                next.f_cost = next.g_cost + get_h_cost(next.state, goal);
                next.plan[next.plan_size] = a.type; 
                next.plan_size++; 
                open_list.push_back(next); 
                push_heap(open_list.begin(), open_list.end(), greater<Node>());
            }
        }
    }
    
    save_plan(key, final_plan); 
    return final_plan;
}

Vector2 get_action_target(int act) 
{
    if(act==CHOP_WOOD_BARE || act==CHOP_WOOD_TOOL) return forest.getPoint();
    if(act==MINE_ORE_BARE || act==MINE_ORE_TOOL) return mine.getPoint();
    if(act==FARM_WHEAT || act==BREW_BEER) return farm.getPoint();
    if(act==CRAFT_TOOL) return smith.getPoint();
    if(act==BAKE_BREAD) return bakery.getPoint();
    if(act==BUILD_BRIDGE) return bridge.getPoint();
    if(act==RENT_BED || act==SLEEP) return inn.getPoint();
    if(act==DRINK_WATER) return well.getPoint();
    if(act==BUY_HOUSE || act==RELAX_HOUSE || act==BUILD_HOUSE_ACT) return suburbs.getPoint();
    if(act==DRINK_BEER) return tavern.getPoint();
    if(act==PAY_TAX_ACT || act==WORK_GUARD) return townhall.getPoint();
    if(act==STUDY) return library.getPoint();
    return market.getPoint();
}

void set_target(AgentRenderData& r, Vector2 target, float speed_mult = 1.0f) 
{
    float dx = target.x - r.pos.x;
    float dy = target.y - r.pos.y;
    float dist = sqrtf(dx*dx + dy*dy); 
    if (dist > 0.1f) 
    {
        r.vx = (dx/dist) * (r.spd * speed_mult);
        r.vy = (dy/dist) * (r.spd * speed_mult);
        r.steps = (int)(dist / (r.spd * speed_mult));
    } 
    else 
    {
        r.steps = 0;
    }
}

int main() 
{
    InitWindow(1400, 900, "V9 Agent Simulation"); 
    SetTargetFPS(60);

    vector<Action> actions = 
    {
        {CHOP_WOOD_BARE, 5, pre_chop_bare, eff_chop_bare}, {CHOP_WOOD_TOOL, 2, pre_chop_tool, eff_chop_tool},
        {MINE_ORE_BARE, 6, pre_mine_bare, eff_mine_bare}, {MINE_ORE_TOOL, 2, pre_mine_tool, eff_mine_tool},
        {FARM_WHEAT, 4, pre_farm, eff_farm}, {CRAFT_TOOL, 3, pre_craft, eff_craft}, {BAKE_BREAD, 3, pre_bake, eff_bake},
        {SELL_WOOD, 1, pre_sell_wood, eff_sell_wood}, {BUY_WOOD, 1, pre_buy_wood, eff_buy_wood},
        {SELL_ORE, 1, pre_sell_ore, eff_sell_ore}, {BUY_ORE, 1, pre_buy_ore, eff_buy_ore},
        {SELL_WHEAT, 1, pre_sell_wheat, eff_sell_wheat}, {BUY_WHEAT, 1, pre_buy_wheat, eff_buy_wheat},
        {SELL_TOOL, 1, pre_sell_tool, eff_sell_tool}, {BUY_TOOL, 1, pre_buy_tool, eff_buy_tool},
        {SELL_BREAD, 1, pre_sell_bread, eff_sell_bread}, {BUY_BREAD, 1, pre_buy_bread, eff_buy_bread},
        {EAT_BREAD, 1, pre_eat, eff_eat}, {RENT_BED, 1, pre_rent, eff_rent}, {SLEEP, 1, pre_sleep, eff_sleep},
        {DRINK_WATER, 1, pre_drink, eff_drink}, {BUILD_BRIDGE, 2, pre_bridge, eff_bridge},
        {BUY_HOUSE, 5, pre_buy_house, eff_buy_house}, {RELAX_HOUSE, 1, pre_relax, eff_relax},
        {BREW_BEER, 4, pre_brew, eff_brew}, {SELL_BEER, 1, pre_sell_beer, eff_sell_beer},
        {BUY_BEER, 1, pre_buy_beer, eff_buy_beer}, {DRINK_BEER, 1, pre_drink_beer, eff_drink_beer},
        {BUILD_HOUSE_ACT, 6, pre_build_h_act, eff_build_h_act}, {PAY_TAX_ACT, 1, pre_pay_tax, eff_pay_tax},
        {WORK_GUARD, 5, pre_work_guard, eff_work_guard}, {STUDY, 5, pre_study, eff_study}
    };

    const int NUM_AGENTS = 524288; 
    vector<AgentRenderData> render_data(NUM_AGENTS);
    vector<AgentLogicData> logic_data(NUM_AGENTS);
    vector<AgentPlan> plan_data(NUM_AGENTS);

    Image agent_img = GenImageColor(1400, 900, BLANK); 
    Texture2D agent_tex = LoadTextureFromImage(agent_img); 
    Color* pixels = (Color*)agent_img.data;

    int total_start_money = 0;

    #pragma omp parallel for schedule(static) reduction(+:total_start_money)
    for(int i=0; i<NUM_AGENTS; i++) 
    {
        logic_data[i].skills = {0,0,0,0,0,0,0};
        
        int profession = get_random(1, 16); 
        
        if (profession == 16) 
        {
            logic_data[i].skills.combat = get_random(8, 10);
        } 
        else 
        {
            int civ_job = get_random(1, 6);
            if(civ_job == 1) logic_data[i].skills.lumberjack = get_random(8, 10);
            else if(civ_job == 2) logic_data[i].skills.miner = get_random(8, 10);
            else if(civ_job == 3) logic_data[i].skills.farmer = get_random(8, 10);
            else if(civ_job == 4) logic_data[i].skills.baker = get_random(8, 10);
            else if(civ_job == 5) logic_data[i].skills.builder = get_random(8, 10);
            else if(civ_job == 6) logic_data[i].skills.smith = get_random(8, 10);
        }

        render_data[i].pos = {(float)get_random(300, 1300), (float)get_random(50, 850)};
        render_data[i].spd = (float)get_random(2, 5); render_data[i].c = WHITE;
        render_data[i].vx = 0; render_data[i].vy = 0; render_data[i].steps = 0;
        
        logic_data[i].action_timer = -1; logic_data[i].needs_plan = true; 
        int start_m = get_random(0,3);
        
        logic_data[i].state = {};
        logic_data[i].state.money = start_m;
        logic_data[i].state.combat_skill = logic_data[i].skills.combat;
        
        total_start_money += start_m; 
        plan_data[i].size = 0; plan_data[i].current_step = 0;
    }

    central_bank_money-=total_start_money;

    int current_frame = 0;

    double prev_plan_ms = 0.0;
    double prev_logic_ms = 0.0;
    double prev_render_ms = 0.0;

    int16_t tax_timer = 0;

    double sum_ai_time = 0.0;
    int measure_count = 0;

    while(!WindowShouldClose()) 
    {
        
        int total_agent_money = 0; 

        int PLAN_FRAMES = 120;
        int agents_per_frame = (NUM_AGENTS + PLAN_FRAMES - 1) / PLAN_FRAMES;
        int start_idx = (current_frame % PLAN_FRAMES) * agents_per_frame;
        int end_idx = min(NUM_AGENTS, start_idx + agents_per_frame);

        if (tax_timer >= 600) 
        { 
            m_wood -= m_wood/10;
            m_ore -= m_ore/10;
            m_wheat -= m_wheat/10;
            m_bread -= m_bread/10;
            m_tools -= m_tools/10;
            m_beer -= m_beer/10;
            tax_timer = 0;
        }
        
        int plans_made_this_frame = 0;
        auto t_start = std::chrono::steady_clock::now();

        #pragma omp parallel for schedule(dynamic,64) reduction(+:plans_made_this_frame)
        for(int i = start_idx; i < end_idx; i++) 
        {
            if(logic_data[i].needs_plan) 
            {
                logic_data[i].state.g_money = central_bank_money; logic_data[i].state.g_wood = m_wood;
                logic_data[i].state.g_ore = m_ore; logic_data[i].state.g_wheat = m_wheat;
                logic_data[i].state.g_tools = m_tools; logic_data[i].state.g_bread = m_bread;
                logic_data[i].state.g_beer = m_beer; logic_data[i].state.g_houses = global_houses;
                logic_data[i].state.g_bridge_wood = global_bridge_wood;

                plan_data[i] = build_plan(logic_data[i].state, logic_data[i].goal, actions, logic_data[i].skills);
                plans_made_this_frame++;
                logic_data[i].needs_plan = false;
                
                if(plan_data[i].size > 0) 
                {
                    set_target(render_data[i], get_action_target(plan_data[i].actions[0]), 1.0f);
                }
            }
        }

        auto t_plan_end = std::chrono::steady_clock::now();

        #pragma omp parallel for schedule(static,64) reduction(+:total_agent_money)
        for(int i = 0; i < NUM_AGENTS; i++) 
        {
            auto& r = render_data[i]; auto& l = logic_data[i]; auto& p = plan_data[i];
            
            total_agent_money += l.state.money;
            
            if (current_frame % 3600 == i % 3600) 
            {
                l.state.paid_tax = false;
            }

            if (current_frame % 60 == i % 60) 
            {
                if(!l.state.is_hungry && !l.state.is_tired && !l.state.is_thirsty) 
                {
                    int rnd = get_random(1, 100); 
                    if(rnd<3) { l.state.is_hungry=true; l.goal=FIX_HUNGER; }
                    else if(rnd<6) { l.state.is_tired=true; l.goal=FIX_TIREDNESS; }
                    else if(rnd<9) { l.state.is_thirsty=true; l.goal=FIX_THIRST; }
                    else {
                        if (!l.state.owns_house && global_houses.load(std::memory_order_relaxed) > 0 && l.state.money < 50) {
                            l.goal = GET_RICH; l.state.target_money = l.state.money+8;
                        }
                        else if (!l.state.owns_house && global_houses.load(std::memory_order_relaxed) > 0 && l.state.money >= 50) {
                            l.goal = BECOME_HOMEOWNER;
                        }
                        else if (!l.state.owns_house && global_houses.load(std::memory_order_relaxed) == 0 && (l.skills.builder >= 8 || l.state.money >= 80)) {
                            l.goal = BECOME_HOMEOWNER;
                        }
                        else if (!l.state.is_educated && l.state.money >= 15 && (l.skills.combat >= 5 || get_random(1,3)==1)) {
                            l.goal = BECOME_EDUCATED; 
                        }
                        else if (!l.state.paid_tax && l.state.money >= 40 && get_random(1,3)==1) {
                            l.goal = PAY_TAXES; 
                        }
                        else if (global_bridge_wood.load(std::memory_order_relaxed) < MAX_BRIDGE && get_random(1,4)==1) { 
                            l.goal=CONTRIBUTE_BRIDGE; l.state.built_bridge=false; 
                        }
                        else { 
                            l.goal=GET_RICH; l.state.target_money = l.state.money + get_random(15, 30); 
                        }
                    }
                }
            } 

            if(p.size > 0 && p.current_step < p.size) {
                if(r.steps > 0) { 
                    r.pos.x += r.vx; r.pos.y += r.vy; r.steps--; r.c = WHITE; 
                } else {
                    int act = p.actions[p.current_step];
                    
                    if(l.action_timer == -1) {
                        if(act==SLEEP || act==RELAX_HOUSE || act==STUDY) l.action_timer = 120; 
                        else if(act==CHOP_WOOD_BARE || act==MINE_ORE_BARE || act==BUILD_HOUSE_ACT) l.action_timer = 60; 
                        else if(act==CHOP_WOOD_TOOL || act==MINE_ORE_TOOL || act==WORK_GUARD) l.action_timer = 30; 
                        else if(act>=SELL_WOOD && act<=BUY_BREAD) l.action_timer = 10; 
                        else l.action_timer = 30; 
                    }

                    if(l.action_timer > 0) {
                        l.action_timer--;
                        if(act==CHOP_WOOD_BARE || act==CHOP_WOOD_TOOL) r.c = GREEN;
                        else if(act==MINE_ORE_BARE || act==MINE_ORE_TOOL) r.c = GRAY;
                        else if(act==FARM_WHEAT || act==BREW_BEER) r.c = YELLOW;
                        else if(act==RELAX_HOUSE || act==BUY_HOUSE || act==BUILD_HOUSE_ACT) r.c = PURPLE;
                        else if(act==SLEEP || act==RENT_BED) r.c = BLUE;
                        else if(act==BUILD_BRIDGE) r.c = DARKBROWN;
                        else if(act==DRINK_BEER) r.c = MAROON;
                        else if(act==WORK_GUARD || act==PAY_TAX_ACT) r.c = GOLD;
                        else if(act==STUDY) r.c = TEAL;
                    } else if(l.action_timer == 0) {
                        bool ok = true;
                        
                        if(act==CHOP_WOOD_BARE) l.state=eff_chop_bare(l.state);
                        else if(act==CHOP_WOOD_TOOL) { l.state=eff_chop_tool(l.state); if(get_random(1,100)<20) l.state.has_tool=false; }
                        else if(act==MINE_ORE_BARE) l.state=eff_mine_bare(l.state);
                        else if(act==MINE_ORE_TOOL) { l.state=eff_mine_tool(l.state); if(get_random(1,100)<20) l.state.has_tool=false; }
                        else if(act==FARM_WHEAT) l.state=eff_farm(l.state);
                        else if(act==CRAFT_TOOL) l.state=eff_craft(l.state);
                        else if(act==BAKE_BREAD) l.state=eff_bake(l.state);
                        else if(act==EAT_BREAD) l.state=eff_eat(l.state); 
                        else if(act==SLEEP) l.state=eff_sleep(l.state); 
                        else if(act==DRINK_WATER) l.state=eff_drink(l.state); 
                        else if(act==RELAX_HOUSE) l.state=eff_relax(l.state);
                        else if(act==BREW_BEER) l.state=eff_brew(l.state);
                        else if(act==DRINK_BEER) l.state=eff_drink_beer(l.state);

                        else if(act==SELL_WOOD){ 
                            if(central_bank_money.load(std::memory_order_relaxed)>=2 && m_wood.load(std::memory_order_relaxed)<100000) { 
                                central_bank_money.fetch_sub(2, std::memory_order_relaxed); 
                                m_wood.fetch_add(1, std::memory_order_relaxed);
                                l.state=eff_sell_wood(l.state); 
                            } else ok=false; 
                        }
                        else if(act==BUY_WOOD){ 
                            if(m_wood.load(std::memory_order_relaxed)>0 && l.state.money>=3){ 
                                central_bank_money.fetch_add(3, std::memory_order_relaxed); 
                                m_wood.fetch_sub(1, std::memory_order_relaxed);
                                l.state=eff_buy_wood(l.state); 
                            } else ok=false; 
                        }
                        else if(act==SELL_ORE){ 
                            if(central_bank_money.load(std::memory_order_relaxed)>=3 && m_ore.load(std::memory_order_relaxed)<50000){ 
                                central_bank_money.fetch_sub(3, std::memory_order_relaxed); 
                                m_ore.fetch_add(1, std::memory_order_relaxed);
                                l.state=eff_sell_ore(l.state); 
                            } else ok=false; 
                        }
                        else if(act==BUY_ORE){ 
                            if(m_ore.load(std::memory_order_relaxed)>0 && l.state.money>=4){ 
                                central_bank_money.fetch_add(4, std::memory_order_relaxed); 
                                m_ore.fetch_sub(1, std::memory_order_relaxed);
                                l.state=eff_buy_ore(l.state); 
                            } else ok=false; 
                        }
                        else if(act==SELL_WHEAT){ 
                            if(central_bank_money.load(std::memory_order_relaxed)>=1 && m_wheat.load(std::memory_order_relaxed)<50000){ 
                                central_bank_money.fetch_sub(1, std::memory_order_relaxed); 
                                m_wheat.fetch_add(1, std::memory_order_relaxed);
                                l.state=eff_sell_wheat(l.state); 
                            } else ok=false; 
                        }
                        else if(act==BUY_WHEAT){ 
                            if(m_wheat.load(std::memory_order_relaxed)>0 && l.state.money>=2){ 
                                central_bank_money.fetch_add(2, std::memory_order_relaxed); 
                                m_wheat.fetch_sub(1, std::memory_order_relaxed);
                                l.state=eff_buy_wheat(l.state); 
                            } else ok=false; 
                        }
                        else if(act==SELL_TOOL){ 
                            if(central_bank_money.load(std::memory_order_relaxed)>=8 && m_tools.load(std::memory_order_relaxed)<20000){ 
                                central_bank_money.fetch_sub(8, std::memory_order_relaxed); 
                                m_tools.fetch_add(1, std::memory_order_relaxed);
                                l.state=eff_sell_tool(l.state); 
                            } else ok=false; 
                        }
                        else if(act==BUY_TOOL){ 
                            if(m_tools.load(std::memory_order_relaxed)>0 && l.state.money>=10){ 
                                central_bank_money.fetch_add(10, std::memory_order_relaxed); 
                                m_tools.fetch_sub(1, std::memory_order_relaxed);
                                l.state=eff_buy_tool(l.state); 
                            } else ok=false; 
                        }
                        else if(act==SELL_BREAD){ 
                            if(central_bank_money.load(std::memory_order_relaxed)>=3 && m_bread.load(std::memory_order_relaxed)<50000){ 
                                central_bank_money.fetch_sub(3, std::memory_order_relaxed); 
                                m_bread.fetch_add(1, std::memory_order_relaxed);
                                l.state=eff_sell_bread(l.state); 
                            } else ok=false; 
                        }
                        else if(act==BUY_BREAD){ 
                            if(m_bread.load(std::memory_order_relaxed)>0 && l.state.money>=4){ 
                                central_bank_money.fetch_add(4, std::memory_order_relaxed); 
                                m_bread.fetch_sub(1, std::memory_order_relaxed);
                                l.state=eff_buy_bread(l.state); 
                            } else ok=false; 
                        }
                        else if(act==SELL_BEER){ 
                            if(central_bank_money.load(std::memory_order_relaxed)>=5 && m_beer.load(std::memory_order_relaxed)<20000){ 
                                central_bank_money.fetch_sub(5, std::memory_order_relaxed); 
                                m_beer.fetch_add(1, std::memory_order_relaxed);
                                l.state=eff_sell_beer(l.state); 
                            } else ok=false; 
                        }
                        else if(act==BUY_BEER){ 
                            if(m_beer.load(std::memory_order_relaxed)>0 && l.state.money>=6){ 
                                central_bank_money.fetch_add(6, std::memory_order_relaxed); 
                                m_beer.fetch_sub(1, std::memory_order_relaxed);
                                l.state=eff_buy_beer(l.state); 
                            } else ok=false; 
                        }

                        else if(act==RENT_BED){ 
                            if(l.state.money>=2) { 
                                central_bank_money.fetch_add(2, std::memory_order_relaxed); 
                                l.state=eff_rent(l.state); 
                            } else ok=false; 
                        } 
                        else if(act==BUY_HOUSE){ 
                            if(global_houses.load(std::memory_order_relaxed)>0 && l.state.money>=50){ 
                                central_bank_money.fetch_add(50, std::memory_order_relaxed); 
                                global_houses.fetch_sub(1, std::memory_order_relaxed);
                                l.state=eff_buy_house(l.state); 
                            } else ok=false; 
                        }
                        else if(act==BUILD_HOUSE_ACT){ 
                            if(central_bank_money.load(std::memory_order_relaxed)>=20){ 
                                central_bank_money.fetch_sub(20, std::memory_order_relaxed); 
                                global_houses.fetch_add(1, std::memory_order_relaxed);
                                l.state=eff_build_h_act(l.state); 
                            } else ok=false;
                        }
                        else if(act==PAY_TAX_ACT){ 
                            if(l.state.money>=10) { 
                                central_bank_money.fetch_add(10, std::memory_order_relaxed); 
                                l.state=eff_pay_tax(l.state); 
                            } else ok=false; 
                        }
                        else if(act==WORK_GUARD){ 
                            if(central_bank_money.load(std::memory_order_relaxed)>=5){ 
                                central_bank_money.fetch_sub(5, std::memory_order_relaxed); 
                                l.state=eff_work_guard(l.state); 
                            } else ok=false; 
                        }
                        else if(act==STUDY){ 
                            if(l.state.money>=15) { 
                                central_bank_money.fetch_add(15, std::memory_order_relaxed); 
                                l.state=eff_study(l.state); 
                            } else ok=false; 
                        }
                        else if(act==BUILD_BRIDGE) { 
                            if(central_bank_money.load(std::memory_order_relaxed)>=5 && global_bridge_wood.load(std::memory_order_relaxed) < MAX_BRIDGE) { 
                                central_bank_money.fetch_sub(10, std::memory_order_relaxed); 
                                global_bridge_wood.fetch_add(1, std::memory_order_relaxed);
                                l.state=eff_bridge(l.state); 
                            } else ok=false; 
                        }
                        l.action_timer = -1; 
                        if(ok) {
                            p.current_step++;
                            if(p.current_step < p.size) set_target(r, get_action_target(p.actions[p.current_step]), 1.0f);
                        } else { p.size = 0; p.current_step = 0; l.needs_plan = true; } 
                    }
                }
            } else { 
                l.needs_plan = true; 
                
                if(r.steps <= 0) {
                    if (current_frame % 60 == i % 60) {
                        if(get_random(1,100)<15) { 
                            Vector2 t = {r.pos.x+get_random(-30,30), r.pos.y+get_random(-30,30)};
                            set_target(r, t, 0.3f); 
                        }
                    }
                }

                if(r.steps > 0) {
                    r.pos.x += r.vx; r.pos.y += r.vy; r.steps--;
                }
                r.c = ColorAlpha(WHITE, 0.3f);
            }
        }

        auto t_logic_end = std::chrono::steady_clock::now();

        BeginDrawing(); ClearBackground({20, 20, 25, 255});
            DrawRectangleRec(river.r, ColorAlpha(BLUE, 0.4f)); 
            float bridge_width = (100.0f * global_bridge_wood) / MAX_BRIDGE;
            DrawRectangle(150 - bridge_width, 430, bridge_width, 40, DARKBROWN); 

            DrawRectangleRec(forest.r, ColorAlpha(DARKGREEN, 0.6f)); DrawText(forest.n, forest.r.x+5, forest.r.y+5, 20, WHITE);
            DrawRectangleRec(mine.r, ColorAlpha(GRAY, 0.6f)); DrawText(mine.n, mine.r.x+5, mine.r.y+5, 20, WHITE);
            DrawRectangleRec(farm.r, ColorAlpha(YELLOW, 0.4f)); DrawText(farm.n, farm.r.x+5, farm.r.y+5, 20, BLACK);
            DrawRectangleRec(inn.r, ColorAlpha(DARKBLUE, 0.6f)); DrawText(inn.n, inn.r.x+5, inn.r.y+5, 20, WHITE);
            DrawRectangleRec(market.r, ColorAlpha(ORANGE, 0.4f)); DrawText(market.n, market.r.x+5, market.r.y+5, 20, BLACK);
            DrawRectangleRec(smith.r, ColorAlpha(BLACK, 0.6f)); DrawText(smith.n, smith.r.x+5, smith.r.y+5, 20, WHITE);
            DrawRectangleRec(bakery.r, ColorAlpha(PINK, 0.6f)); DrawText(bakery.n, bakery.r.x+5, bakery.r.y+5, 20, BLACK);
            DrawRectangleRec(well.r, ColorAlpha(SKYBLUE, 0.6f)); DrawText(well.n, well.r.x+5, well.r.y+5, 20, BLACK);
            DrawRectangleRec(suburbs.r, ColorAlpha(PURPLE, 0.6f)); DrawText(suburbs.n, suburbs.r.x+5, suburbs.r.y+5, 20, WHITE);
            DrawRectangleRec(tavern.r, ColorAlpha(MAROON, 0.6f)); DrawText(tavern.n, tavern.r.x+5, tavern.r.y+5, 20, WHITE);
            DrawRectangleRec(townhall.r, ColorAlpha(GOLD, 0.4f)); DrawText(townhall.n, townhall.r.x+5, townhall.r.y+5, 20, BLACK);
            DrawRectangleRec(library.r, ColorAlpha(TEAL, 0.6f)); DrawText(library.n, library.r.x+5, library.r.y+5, 20, WHITE);

            memset(pixels, 0, 1400 * 900 * sizeof(Color));

            #pragma omp parallel for schedule(static)
            for(int i = 0; i < NUM_AGENTS; i++) 
            {
                int px = (int)render_data[i].pos.x;
                int py = (int)render_data[i].pos.y;
                
                if(px >= 0 && px < 1400 && py >= 0 && py < 900)
                {
                    pixels[py * 1400 + px] = render_data[i].c; 
                }
            }

            UpdateTexture(agent_tex, pixels);
            DrawTexture(agent_tex, 0, 0, WHITE);

            DrawRectangle(10, 10, 320, 300, ColorAlpha(BLACK, 0.8f)); 
            DrawText("--- MACRO ECONOMY ---", 20, 20, 20, LIGHTGRAY);
            DrawText(TextFormat("Central Bank (Money): %d", central_bank_money.load()), 20, 50, 20, GOLD);
            DrawText(TextFormat("Market Wood : %d/100000", m_wood.load()), 20, 80, 20, GREEN);
            DrawText(TextFormat("Market Ore  : %d/50000", m_ore.load()), 20, 105, 20, GRAY);
            DrawText(TextFormat("Market Wheat: %d/50000", m_wheat.load()), 20, 130, 20, YELLOW);
            DrawText(TextFormat("Market Tools: %d/20000", m_tools.load()), 20, 155, 20, LIGHTGRAY);
            DrawText(TextFormat("Market Bread: %d/50000", m_bread.load()), 20, 180, 20, PINK);
            DrawText(TextFormat("Market Beer : %d/20000", m_beer.load()), 20, 205, 20, MAROON);
            DrawText(TextFormat("Available Houses: %d", global_houses.load()), 20, 230, 20, PURPLE);
            DrawText(TextFormat("Bridge Wood : %d / %d", global_bridge_wood.load(), MAX_BRIDGE), 20, 255, 20, DARKBROWN); 
            
            DrawText(TextFormat("AI Logic Time: %.2f ms (GOAP Plan: %.2f ms)", prev_logic_ms, prev_plan_ms), 10, 745, 20, ORANGE);
            DrawText(TextFormat("Render Time  : %.2f ms", prev_render_ms), 10, 770, 20, GREEN);
            
            DrawText(TextFormat("Thread Count : %d   |   Agents: %d", omp_get_max_threads(), NUM_AGENTS), 10, 795, 20, SKYBLUE);
            
            DrawText(TextFormat("Plans Created/Frame: %d", plans_made_this_frame), 10, 820, 20, PINK);
            DrawText(TextFormat("Total M: %d (Bank) + %d (Ag) = %d", central_bank_money.load(), total_agent_money, central_bank_money.load()+total_agent_money), 10, 845, 20, WHITE);
            
            DrawFPS(10, 870);

        EndDrawing();

        auto t_render_end = std::chrono::steady_clock::now();
        
        prev_plan_ms = std::chrono::duration<double, std::milli>(t_plan_end - t_start).count();
        prev_logic_ms = std::chrono::duration<double, std::milli>(t_logic_end - t_start).count();
        prev_render_ms = std::chrono::duration<double, std::milli>(t_render_end - t_logic_end).count();

        if (current_frame > 20 && measure_count < 500) {
            sum_ai_time += prev_logic_ms; 
            measure_count++;
            
            if (measure_count == 500) {
                double avg_time = sum_ai_time / 500.0;
                cout << "=======================================\n";
                cout << "Agents : " << NUM_AGENTS << "\n";
                cout << "Threads: " << omp_get_max_threads() << "\n";
                cout << "AVG AI Time : " << avg_time << " ms\n";
                cout << "=======================================\n";
            }
        }
        
        current_frame++;
    }
    CloseWindow(); return 0;
}