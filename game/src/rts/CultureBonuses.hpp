#pragma once

/**
 * @file CultureBonuses.hpp
 * @brief Detailed documentation of culture-specific bonuses and mechanics
 *
 * This file provides comprehensive documentation of each culture's
 * strengths, weaknesses, unique abilities, and recommended playstyles.
 * Use this as a reference for game balance and player guidance.
 */

namespace Vehement {
namespace RTS {

/**
 * =============================================================================
 * FORTRESS CULTURE - The Immovable Bastion
 * =============================================================================
 *
 * Theme: Medieval European castle architecture
 * Playstyle: Turtle defense, hold chokepoints
 *
 * BONUSES:
 * - +50% Wall HP
 *   Fortress walls are legendary for their durability. Stone walls reinforced
 *   with iron bands can withstand sustained zombie attacks far longer than
 *   other cultures' defenses.
 *
 * - +25% Tower Damage
 *   Castle towers are designed for maximum effectiveness. Higher vantage points
 *   and superior construction allow for deadlier defensive fire.
 *
 * - +20% Building HP
 *   All Fortress structures are built to last. Stone foundations and thick
 *   walls provide extra durability against all forms of attack.
 *
 * - +15% Defense
 *   Units trained in Fortress settlements wear heavier armor and use superior
 *   defensive tactics learned from generations of siege warfare.
 *
 * PENALTIES:
 * - -20% Build Speed
 *   Quality construction takes time. Stone masonry and proper fortification
 *   cannot be rushed without compromising structural integrity.
 *
 * - -5% Unit Speed
 *   Heavy armor and equipment slow down Fortress units compared to lighter
 *   equipped cultures.
 *
 * UNIQUE ABILITIES:
 * 1. Stone Walls - Walls have +50% HP and resist fire damage
 * 2. Castle Keep - Main building provides defensive aura to nearby structures
 * 3. Fortified Towers - Towers deal +25% damage and have extended range
 * 4. Hold the Line - Units near walls gain +20% defense
 *
 * UNIQUE BUILDING: Castle
 * - Massive stone fortress that serves as ultimate headquarters
 * - Provides garrison space for many units
 * - Cannot be destroyed while any wall segment remains
 *
 * RECOMMENDED STRATEGY:
 * - Establish defensive perimeter early
 * - Create layered defenses with multiple wall rings
 * - Use towers to create overlapping fields of fire
 * - Hold chokepoints and let zombies break against your walls
 *
 * =============================================================================
 * BUNKER CULTURE - Modern Military Superiority
 * =============================================================================
 *
 * Theme: Contemporary military installations
 * Playstyle: High-tech defense, automated systems
 *
 * BONUSES:
 * - +35% Turret/Tower Damage
 *   Advanced targeting systems and superior weapons make Bunker defensive
 *   installations devastatingly effective.
 *
 * - +25% Defense (in structures)
 *   Reinforced concrete and steel plating provide exceptional protection
 *   for personnel inside Bunker structures.
 *
 * - +20% Detection Range
 *   Radar systems, motion sensors, and surveillance equipment give early
 *   warning of approaching threats.
 *
 * - +15% Healing
 *   Military medical facilities with trained personnel provide superior
 *   treatment for wounded units.
 *
 * - +10% Production Speed
 *   Standardized military equipment and efficient processes speed production.
 *
 * PENALTIES:
 * - +15% Build Cost
 *   High-tech materials and precision construction increase resource costs.
 *
 * UNIQUE ABILITIES:
 * 1. Automated Turrets - Turrets fire independently without operators
 * 2. Reinforced Concrete - Buildings resist explosive damage
 * 3. Kill Zone - Overlapping turret fire deals bonus damage
 * 4. Emergency Lockdown - All buildings become invulnerable briefly (cooldown)
 *
 * RECOMMENDED STRATEGY:
 * - Prioritize turret placement and overlapping fields of fire
 * - Use detection systems for early warning
 * - Invest in research to unlock advanced weapons
 * - Create kill zones where multiple turrets can engage targets
 *
 * =============================================================================
 * NOMAD CULTURE - The Swift Wanderers
 * =============================================================================
 *
 * Theme: Mongol/Central Asian yurt camps
 * Playstyle: Mobility, rapid expansion, strategic relocation
 *
 * BONUSES:
 * - +50% Build Speed
 *   Nomads have perfected quick assembly of their traditional structures.
 *   What takes other cultures hours takes Nomads minutes.
 *
 * - +100% Packing Speed
 *   The signature ability - Nomad buildings can be packed into wagons and
 *   moved to new locations, then unpacked rapidly.
 *
 * - +20% Unit Speed
 *   Nomad warriors are trained from birth in horsemanship and swift movement.
 *
 * - +30% Caravan Speed
 *   Trade caravans move quickly across terrain, enabling rapid resource flow.
 *
 * - +15% Gathering Speed
 *   Efficient nomadic practices minimize waste during resource collection.
 *
 * PENALTIES:
 * - -25% Building HP
 *   Portable structures sacrifice durability for mobility.
 *
 * - -30% Wall HP
 *   Temporary fortifications cannot match permanent construction.
 *
 * UNIQUE ABILITIES:
 * 1. Pack Up - Buildings can be packed into wagons and relocated
 * 2. Swift Assembly - Buildings are constructed 50% faster
 * 3. Caravan Masters - Trade caravans move 30% faster and carry more
 * 4. Escape Artists - Units gain speed boost when retreating
 *
 * UNIQUE BUILDINGS: Yurt, Mobile Workshop
 * - Yurt: Basic housing that packs/unpacks instantly
 * - Mobile Workshop: Production facility on wheels
 *
 * RECOMMENDED STRATEGY:
 * - Exploit hit-and-run tactics
 * - Relocate base when overwhelmed rather than fighting to the death
 * - Use speed advantage to gather resources across large areas
 * - Establish multiple small camps rather than one large base
 *
 * =============================================================================
 * SCAVENGER CULTURE - Masters of Making Do
 * =============================================================================
 *
 * Theme: Post-apocalyptic improvisation
 * Playstyle: Zerg rush, cheap units, expendable structures
 *
 * BONUSES:
 * - -40% Build Cost
 *   Scavengers build from salvage and scrap, dramatically reducing costs.
 *
 * - +35% Build Speed
 *   Makeshift construction doesn't require precision - just hammer it together.
 *
 * - +25% Gathering Speed
 *   Expert scavengers find value in what others discard.
 *
 * - +50% Repair Speed
 *   When everything is held together with tape and hope, quick repairs are
 *   essential for survival.
 *
 * - +20% Storage Capacity
 *   Hoarder mentality means every Scavenger settlement maximizes storage.
 *
 * PENALTIES:
 * - -35% Building HP
 *   Improvised construction doesn't hold up under sustained attack.
 *
 * - -40% Wall HP
 *   Scrap metal barriers are better than nothing, but not by much.
 *
 * - -15% Defense
 *   Makeshift armor provides minimal protection.
 *
 * UNIQUE ABILITIES:
 * 1. Salvage - Destroyed buildings return 50% resources
 * 2. Improvised Defense - Can build anywhere without foundations
 * 3. Scrap Armor - Units gain temporary armor from nearby debris
 * 4. Rapid Reconstruction - Rebuild destroyed buildings instantly (cooldown)
 *
 * RECOMMENDED STRATEGY:
 * - Build many cheap structures rather than few expensive ones
 * - Accept losses and rebuild quickly
 * - Overwhelm with numbers rather than quality
 * - Use Salvage ability to recycle destroyed buildings
 *
 * =============================================================================
 * MERCHANT CULTURE - Gold Wins Wars
 * =============================================================================
 *
 * Theme: Silk Road trading posts
 * Playstyle: Economic domination, hire mercenaries
 *
 * BONUSES:
 * - +30% Trade Profits
 *   Expert negotiators and established trade routes maximize profits.
 *
 * - +25% Caravan Speed
 *   Well-maintained roads and experienced drivers speed deliveries.
 *
 * - +40% Storage Capacity
 *   Warehouses designed for commerce hold vast quantities of goods.
 *
 * - +15% Production Speed
 *   Efficient workflows and quality tools improve output.
 *
 * - +10% Gathering Speed
 *   Access to specialized equipment improves resource collection.
 *
 * - -10% Build Cost
 *   Bulk purchasing and trade connections reduce material costs.
 *
 * PENALTIES:
 * - -10% Defense
 *   Merchants are traders, not warriors. Combat is not their forte.
 *
 * UNIQUE ABILITIES:
 * 1. Trade Routes - Establish profitable routes to other settlements
 * 2. Bazaar Discounts - Buy rare items at reduced prices
 * 3. Hire Mercenaries - Spend gold to instantly recruit units
 * 4. Diplomatic Immunity - Caravans cannot be attacked by NPC factions
 *
 * UNIQUE BUILDING: Bazaar
 * - Central trading hub with access to exotic goods
 * - Generates passive income from NPC traders
 * - Unlocks mercenary recruitment
 *
 * RECOMMENDED STRATEGY:
 * - Focus on economy first, military second
 * - Establish trade routes for passive income
 * - Use gold to hire mercenaries for defense
 * - Trade for rare resources rather than gathering them
 *
 * =============================================================================
 * INDUSTRIAL CULTURE - The Factories Never Sleep
 * =============================================================================
 *
 * Theme: Victorian-era industrial revolution
 * Playstyle: Mass production, resource conversion
 *
 * BONUSES:
 * - +50% Production Speed
 *   Assembly lines and automation dramatically increase output.
 *
 * - +20% Gathering Speed
 *   Industrial mining and harvesting equipment improves efficiency.
 *
 * - +15% Build Speed
 *   Prefabricated components speed construction.
 *
 * - +30% Repair Speed
 *   Standardized parts make repairs straightforward.
 *
 * PENALTIES:
 * - +10% Build Cost
 *   Industrial construction requires more resources upfront.
 *
 * - -15% Detection Range
 *   Noise and pollution mask incoming threats.
 *
 * - -30% Stealth
 *   Smokestacks and machinery make concealment impossible.
 *
 * UNIQUE ABILITIES:
 * 1. Assembly Line - Produce multiple units simultaneously
 * 2. Automation - Factories operate without workers
 * 3. Industrial Output - +50% resource production from all sources
 * 4. Emergency Production - Temporarily double output (causes breakdown)
 *
 * UNIQUE BUILDING: Factory
 * - Massive production facility for all unit types
 * - Can produce multiple queues simultaneously
 * - Requires significant power
 *
 * RECOMMENDED STRATEGY:
 * - Build Factory as soon as possible
 * - Focus on resource income to feed production
 * - Outproduce enemies rather than outfighting them
 * - Accept that you'll attract attention - be ready for it
 *
 * =============================================================================
 * UNDERGROUND CULTURE - What Lies Beneath
 * =============================================================================
 *
 * Theme: Dwarven mines and bunkers
 * Playstyle: Stealth, hidden bases, ambush tactics
 *
 * BONUSES:
 * - +100% Stealth (Hidden from fog of war)
 *   Underground bases cannot be seen on the surface. Enemies must physically
 *   discover hidden entrances to find your settlement.
 *
 * - +50% Defense (in bunkers)
 *   Fighting in cramped tunnels gives defenders enormous advantages.
 *
 * - +30% Detection Range
 *   Tunnel networks allow scouts to observe surface activity undetected.
 *
 * - +25% Storage Capacity
 *   Underground vaults provide secure, expandable storage.
 *
 * PENALTIES:
 * - -15% Build Speed
 *   Excavation and underground construction takes longer.
 *
 * - -10% Surface Movement Speed
 *   Underground dwellers are slower on the surface.
 *
 * - -20% Vision Range
 *   Limited surface presence reduces visible scouting.
 *
 * UNIQUE ABILITIES:
 * 1. Hidden Bases - Buildings invisible on enemy fog of war
 * 2. Tunnel Network - Units can travel between connected buildings
 * 3. Ambush - Units emerging from tunnels deal bonus damage
 * 4. Collapse Tunnel - Destroy tunnel to damage pursuing enemies
 *
 * UNIQUE BUILDING: Hidden Entrance
 * - Surface access point that appears as natural terrain
 * - Multiple entrances allow strategic flexibility
 * - Can be sealed in emergencies
 *
 * RECOMMENDED STRATEGY:
 * - Keep your base hidden as long as possible
 * - Use tunnels for surprise attacks
 * - Create multiple hidden entrances for escape routes
 * - Fight in your tunnels where you have advantage
 *
 * =============================================================================
 * FOREST CULTURE - One With Nature
 * =============================================================================
 *
 * Theme: Elven woodland settlements
 * Playstyle: Guerrilla warfare, ambush, terrain advantage
 *
 * BONUSES:
 * - +60% Stealth
 *   Forest settlements blend with natural surroundings.
 *
 * - +30% Vision Range
 *   Expert scouts and elevated positions provide excellent surveillance.
 *
 * - +15% Unit Speed
 *   Pathfinders know every shortcut through the wilderness.
 *
 * - +20% Attack (ambush bonus)
 *   First strike from concealment deals devastating damage.
 *
 * - +20% Gathering Speed
 *   Sustainable harvesting and foraging expertise.
 *
 * - -15% Build Cost
 *   Natural materials are abundant and free.
 *
 * PENALTIES:
 * - -15% Building HP
 *   Wooden structures are less durable than stone or metal.
 *
 * - -20% Wall HP
 *   Wooden palisades provide limited protection.
 *
 * UNIQUE ABILITIES:
 * 1. Camouflage - Buildings harder to spot in forested areas
 * 2. Ambush Tactics - First attack from stealth deals 2x damage
 * 3. Nature's Bounty - Farms produce 30% more food
 * 4. Pathfinders - Units ignore terrain movement penalties
 *
 * RECOMMENDED STRATEGY:
 * - Build in forested areas for maximum camouflage
 * - Use scouts to find enemies before they find you
 * - Ambush enemies in terrain that favors your units
 * - Avoid direct confrontation; strike and fade
 *
 * =============================================================================
 */

/**
 * @brief Culture balance summary
 *
 * TIER LIST (based on different playstyles):
 *
 * Defensive (Hold position):
 * S: Fortress, Bunker
 * A: Underground
 * B: Industrial, Scavenger
 * C: Merchant, Forest, Nomad
 *
 * Economic (Resource focus):
 * S: Merchant, Industrial
 * A: Scavenger, Nomad
 * B: Forest
 * C: Fortress, Bunker, Underground
 *
 * Aggressive (Attack-focused):
 * S: Industrial, Nomad
 * A: Forest, Scavenger
 * B: Bunker, Merchant
 * C: Fortress, Underground
 *
 * Stealth (Hidden operations):
 * S: Underground, Forest
 * A: Nomad
 * B: Scavenger
 * C: Merchant, Fortress, Bunker, Industrial
 *
 * COUNTER RELATIONSHIPS:
 * - Fortress counters Scavenger (walls stop swarms)
 * - Nomad counters Fortress (can't chase mobile base)
 * - Underground counters Bunker (can't shoot what you can't see)
 * - Industrial counters Nomad (outproduces mobility advantage)
 * - Merchant counters Industrial (economic warfare)
 * - Forest counters Merchant (ambush caravans)
 * - Scavenger counters Underground (overwhelm hiding spots)
 * - Bunker counters Forest (automated defenses ignore stealth)
 */

} // namespace RTS
} // namespace Vehement
