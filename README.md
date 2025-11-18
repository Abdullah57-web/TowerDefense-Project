dsa project report 
tower defence game 
Course Code: CT-159 
Course Instructors: Miss Samia Masood Awan and Miss Sahar 

Group Members: 
Muhammad Taqi  (Group Lead)      (CT-24027) 
Uzair Nadeem    (CT-24037)                                 
Muhammad Abdullah    (CT-24026)                   
Muhammad Ibtissam Aslam        (CT-24020) 
  

   
PROJECT REPORT 


1. Project Description 
This project is a Tower Defence game where the player defends their tower against waves of 
enemy units. 
The game features: 
• Player and enemy towers with health and attack logic. 
• Units (Knight, Archer, Giant, Wizard) spawning for both sides. 
• A freeze ability to temporarily stop enemy units. 
• Wave-based enemy progression. 
• Elixir-based unit spawning for the player. 
• A 2-minute game timer. 
• Graphical representation using shapes and colors. 
The game combines strategy, resource management, and real-time action. 


2. Data Structures Used 
a) Classes 
Classes form the backbone of this game’s design, encapsulating attributes and behaviors for 
different entities. This keeps the code modular, readable, and easy to extend. 
Tower 
The Tower class manages position, health, damage, attack rate, and target selection. Towers 
defend bases and interact with units dynamically, attacking the most relevant targets and 
providing core defensive gameplay. 
Unit 
The Unit class represents Knights, Archers, Giants, and Wizards. It handles movement, 
health, damage, and status effects such as freeze. Units act autonomously while following 
game rules, forming the primary offensive and defensive elements in the game. 
Projectile 
The Projectile class tracks ranged attacks and visual effects. It stores start and end positions, 
color, and progress to render smooth movement and combat animations, providing clear 
feedback for attacks. 
GameWave 
The GameWave class manages enemy wave composition, spawn rates, cooldowns, and links 
to the next wave. It enables sequential wave progression and supports dynamic difficulty by 
adjusting spawn parameters as waves loop. 
b) Linked List 
Linked lists manage dynamic collections of units and waves. Units are frequently spawned 
and removed, and linked lists allow efficient insertion and deletion without shifting 
elements. Wave progression uses linked lists to link waves sequentially, supporting smooth 
transitions and increasing difficulty. 
c) Vectors 
Vectors store projectiles and wave unit configurations. They allow dynamic resizing, efficient 
iteration, and quick access to elements, making it easy to manage active projectiles and 
wave data. 
d) Priority Queue 
Towers use priority queues to select targets based on proximity or priority. This ensures 
efficient targeting without scanning the entire battlefield, improving performance and 
gameplay logic. 


3. Summary 
The Tower Defence game demonstrates the effective use of multiple data structures for 
gameplay. Classes provide design for towers, units, projectiles, and game logic, while linked 
lists efficiently handle dynamic unit management. Vectors store projectiles and wave 
templates for easy iteration, and priority queues ensure optimal targeting by towers. 
Constants, primitive variables, and strings support game state tracking, visual 
representation, and UI messages. Overall, the project integrates strategy, resource 
management, and object-oriented design to create a responsive and scalable game system. 

The end. 
