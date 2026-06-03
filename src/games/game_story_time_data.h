/*
 * Morse Story Time - Story Data
 *
 * Story content stored in PROGMEM for memory efficiency.
 * Ported from the Morse Story Time web application.
 *
 * Stories organized by difficulty:
 * - Tutorial (1 story): Very short intro
 * - Easy (10 stories): ~50-100 words
 * - Medium (8 stories): ~100-200 words
 * - Hard (4 stories): ~150-250 words
 * - Expert (2 stories): ~200+ words
 */

#ifndef GAME_STORY_TIME_DATA_H
#define GAME_STORY_TIME_DATA_H

#include "game_story_time.h"

// ============================================
// TUTORIAL STORY
// ============================================

const StoryData story_tutorial PROGMEM = {
    "tutorial",
    "Welcome to Morse",
    STORY_TUTORIAL,
    25,
    "Hello. My name is Sam. I have a brown dog named Max. Max is three years old. We live in a blue house.",
    {
        {"What is the person's name?", {"Tom", "Sam", "Dan", "Jim"}, 1},
        {"What color is the dog?", {"Black", "White", "Brown", "Golden"}, 2},
        {"What is the dog's name?", {"Rex", "Buddy", "Max", "Duke"}, 2},
        {"How old is Max?", {"Two years old", "Three years old", "Four years old", "Five years old"}, 1},
        {"What color is the house?", {"Red", "White", "Blue", "Green"}, 2}
    }
};

// ============================================
// EASY STORIES
// ============================================

const StoryData story_red_balloon PROGMEM = {
    "red-balloon",
    "The Red Balloon",
    STORY_EASY,
    98,
    "Emma went to the park on Saturday morning. She wore her yellow dress and white shoes. At the park, she saw a man selling balloons. He had blue, green, and red balloons. Emma wanted the red one. It cost two dollars. Her mom gave her the money. Emma held the balloon tight. A bird flew by and scared her. She let go of the string. The balloon floated up into the sky. Emma was sad at first. Then she smiled and waved goodbye to her red balloon.",
    {
        {"What day did Emma go to the park?", {"Sunday", "Saturday", "Friday", "Monday"}, 1},
        {"What color was Emma's dress?", {"Red", "Blue", "Yellow", "Pink"}, 2},
        {"What color balloon did Emma want?", {"Blue", "Green", "Yellow", "Red"}, 3},
        {"How much did the balloon cost?", {"One dollar", "Two dollars", "Three dollars", "Five dollars"}, 1},
        {"What scared Emma?", {"A dog", "A bird", "Thunder", "A car"}, 1}
    }
};

const StoryData story_grandmas_kitchen PROGMEM = {
    "grandmas-kitchen",
    "Grandma's Kitchen",
    STORY_EASY,
    112,
    "Every Sunday, Jake visits his grandma. She lives on Oak Street in a small white house. Today they made chocolate chip cookies. Grandma got out her big red bowl and a wooden spoon. Jake added two cups of flour and one cup of sugar. Grandma cracked three eggs into the bowl. They mixed everything together. Jake put in the chocolate chips. They made twenty four cookies and put them in the oven. The cookies baked for twelve minutes. The kitchen smelled wonderful. Jake ate five cookies while they were still warm. He took ten cookies home in a paper bag for his sister.",
    {
        {"What day does Jake visit grandma?", {"Saturday", "Sunday", "Friday", "Monday"}, 1},
        {"What street does grandma live on?", {"Maple Street", "Pine Street", "Oak Street", "Elm Street"}, 2},
        {"What color was grandma's bowl?", {"Blue", "Green", "Yellow", "Red"}, 3},
        {"How many eggs did grandma crack?", {"Two", "Three", "Four", "Five"}, 1},
        {"How many cookies did Jake eat?", {"Three", "Four", "Five", "Six"}, 2}
    }
};

const StoryData story_lost_puppy PROGMEM = {
    "lost-puppy",
    "The Lost Puppy",
    STORY_EASY,
    125,
    "On Tuesday afternoon, Mia found a small black puppy near the school. The puppy wore a green collar with a silver tag. The tag said the name was Pepper. It also had a phone number. Mia took Pepper to her house on Third Avenue. She gave the puppy water in a blue bowl. Then she called the number on the tag. A woman named Mrs. Chen answered. She was so happy. Mrs. Chen lived two blocks away on Fifth Avenue. She came to get Pepper in fifteen minutes. Mrs. Chen gave Mia twenty dollars as a thank you. She also gave Mia a box of chocolates. Mia was glad she could help.",
    {
        {"What day did Mia find the puppy?", {"Monday", "Tuesday", "Wednesday", "Thursday"}, 1},
        {"What color was the puppy?", {"Brown", "White", "Black", "Golden"}, 2},
        {"What was the puppy's name?", {"Shadow", "Pepper", "Lucky", "Spot"}, 1},
        {"What color was the collar?", {"Red", "Blue", "Green", "Brown"}, 2},
        {"How much did Mrs. Chen give Mia?", {"Ten dollars", "Fifteen dollars", "Twenty dollars", "Twenty five dollars"}, 2}
    }
};

const StoryData story_birthday_cake PROGMEM = {
    "birthday-cake",
    "The Birthday Cake",
    STORY_EASY,
    105,
    "Today is Lily's seventh birthday. She woke up at eight in the morning. Mom made pancakes for breakfast with strawberries on top. Dad gave Lily a big box wrapped in purple paper. Inside was a stuffed rabbit with long white ears. Lily named it Snowball. After lunch, six friends came to the party. They played games in the backyard. The best part was the cake. It was chocolate with pink frosting. There were seven candles on top. Lily made a wish and blew them all out. Everyone sang the birthday song. Lily said it was the best birthday ever.",
    {
        {"How old is Lily turning?", {"Six", "Seven", "Eight", "Nine"}, 1},
        {"What color was the wrapping paper?", {"Blue", "Pink", "Purple", "Yellow"}, 2},
        {"What did Lily name the rabbit?", {"Cotton", "Fluffy", "Snowball", "Bunny"}, 2},
        {"How many friends came to the party?", {"Four", "Five", "Six", "Seven"}, 2},
        {"What flavor was the cake?", {"Vanilla", "Strawberry", "Chocolate", "Lemon"}, 2}
    }
};

const StoryData story_library_visit PROGMEM = {
    "library-visit",
    "The Library Visit",
    STORY_EASY,
    95,
    "Noah went to the library with his dad on Wednesday. The library was on Pine Street next to the post office. Noah returned two books about dinosaurs. Then he looked for new books to read. A librarian named Mrs. Bell helped him. She had curly gray hair and wore glasses. Noah picked three books about space. One book had pictures of Mars. Dad checked out a cookbook. They got library cards that were bright orange. The library closed at six. Noah read his new book in the car ride home.",
    {
        {"What day did Noah go to the library?", {"Monday", "Tuesday", "Wednesday", "Thursday"}, 2},
        {"What street was the library on?", {"Oak Street", "Maple Street", "Pine Street", "Elm Street"}, 2},
        {"What was the librarian's name?", {"Mrs. Bell", "Mrs. Clark", "Mrs. Smith", "Mrs. Jones"}, 0},
        {"How many space books did Noah pick?", {"Two", "Three", "Four", "Five"}, 1},
        {"What color were the library cards?", {"Blue", "Green", "Orange", "Yellow"}, 2}
    }
};

const StoryData story_pet_shop PROGMEM = {
    "pet-shop",
    "The Pet Shop",
    STORY_EASY,
    108,
    "After school on Friday, Jack visited the pet shop with his mom. The shop was called Happy Pets and had a green sign. Inside there were many animals. Jack saw eight puppies in a big pen. They were playing with a red ball. There were also five kittens sleeping in a basket. Jack liked the fish tanks best. One tank had twenty goldfish swimming around. Another tank had blue and yellow fish. The owner was a man named Mr. Davis. He let Jack feed the fish. Jack used a small silver spoon. Mom bought Jack a pet hamster in a white cage. Jack named it Peanut.",
    {
        {"What day did Jack visit the pet shop?", {"Thursday", "Friday", "Saturday", "Sunday"}, 1},
        {"What was the shop called?", {"Pet World", "Happy Pets", "Animal House", "Pet Palace"}, 1},
        {"How many puppies were in the pen?", {"Six", "Seven", "Eight", "Nine"}, 2},
        {"What was the owner's name?", {"Mr. Brown", "Mr. Davis", "Mr. Wilson", "Mr. Clark"}, 1},
        {"What did Jack name his hamster?", {"Fluffy", "Peanut", "Buddy", "Squeaky"}, 1}
    }
};

const StoryData story_dragons_treasure PROGMEM = {
    "dragons-treasure",
    "The Dragon's Treasure",
    STORY_EASY,
    52,
    "Kira the brave climbed the dark mountain. Inside a cave she found a sleeping red dragon named Ember. Around the dragon lay piles of gold coins and seven glowing gems. Kira quietly took one green gem. The dragon snored loudly. She tiptoed out and ran back to her village.",
    {
        {"What was the adventurer's name?", {"Luna", "Kira", "Maya", "Rose"}, 1},
        {"What color was the dragon?", {"Blue", "Green", "Red", "Black"}, 2},
        {"What was the dragon's name?", {"Blaze", "Ember", "Flame", "Ash"}, 1},
        {"How many glowing gems were there?", {"Five", "Six", "Seven", "Eight"}, 2},
        {"What color gem did Kira take?", {"Red", "Blue", "Green", "Purple"}, 2}
    }
};

const StoryData story_wizards_tower PROGMEM = {
    "wizards-tower",
    "The Wizard's Tower",
    STORY_EASY,
    51,
    "Young Theo climbed the tall stone tower to meet the wizard Aldric. The old wizard had a long white beard and wore a purple robe. He gave Theo a dusty spellbook with fifty pages. Theo learned his first spell. He made a small blue flame dance in his hand.",
    {
        {"What was the apprentice's name?", {"Max", "Theo", "Finn", "Cole"}, 1},
        {"What was the wizard's name?", {"Merrick", "Aldric", "Galen", "Thorin"}, 1},
        {"What color was the wizard's robe?", {"Blue", "Red", "Purple", "Black"}, 2},
        {"How many pages did the spellbook have?", {"Thirty", "Forty", "Fifty", "Sixty"}, 2},
        {"What color was the flame Theo made?", {"Red", "Orange", "Blue", "Green"}, 2}
    }
};

const StoryData story_fox_grapes PROGMEM = {
    "fox-grapes",
    "The Fox and the Grapes",
    STORY_EASY,
    68,
    "A hungry fox saw some juicy grapes hanging from a vine high on a tree. The grapes were dark purple and looked very sweet. The fox jumped again and again trying to reach them. He jumped twelve times but could not get them. Finally, the fox walked away and said the grapes were probably sour anyway. This is how some people pretend they do not want what they cannot have.",
    {
        {"What did the fox see?", {"Apples", "Grapes", "Oranges", "Pears"}, 1},
        {"What color were the grapes?", {"Green", "Red", "Purple", "Yellow"}, 2},
        {"How many times did the fox jump?", {"Ten", "Twelve", "Fifteen", "Twenty"}, 1},
        {"What did the fox say about the grapes?", {"They were too high", "They were sour", "They were rotten", "They were too small"}, 1},
        {"Where were the grapes?", {"On a bush", "On the ground", "On a vine", "In a basket"}, 2}
    }
};

const StoryData story_crow_pitcher PROGMEM = {
    "crow-pitcher",
    "The Crow and the Pitcher",
    STORY_EASY,
    85,
    "A thirsty crow found a tall pitcher with some water at the bottom. The pitcher was too narrow and the water was too low for the crow to drink. The clever crow thought of a plan. She picked up small stones one by one and dropped them into the pitcher. After dropping fifteen stones the water rose high enough. The crow finally got her drink. This story teaches us that with patience and clever thinking we can solve difficult problems.",
    {
        {"What did the crow find?", {"A bowl", "A pitcher", "A cup", "A bucket"}, 1},
        {"What was the problem?", {"No water", "Water too low", "Water too hot", "Pitcher broken"}, 1},
        {"What did the crow use?", {"Sticks", "Leaves", "Stones", "Sand"}, 2},
        {"How many stones did she drop?", {"Ten", "Twelve", "Fifteen", "Twenty"}, 2},
        {"What lesson does this teach?", {"Be strong", "Be clever", "Be fast", "Be quiet"}, 1}
    }
};

// ============================================
// MEDIUM STORIES
// ============================================

const StoryData story_old_radio PROGMEM = {
    "old-radio",
    "The Old Radio",
    STORY_MEDIUM,
    198,
    "Ben visited his grandpa every summer in the town of Millbrook. This July, he found something interesting in the attic. It was an old ham radio from nineteen sixty two. The radio was dusty and brown with silver knobs. Grandpa smiled when he saw it. He said he used to talk to people all over the world with this radio. They brought it down to the garage and cleaned it. Grandpa plugged it in. After a few minutes, it started to work. They heard voices speaking in Spanish and then in French. Ben was amazed. Grandpa showed Ben how to use the microphone. Ben said hello and gave his name. A man from Canada answered back. His name was Robert and he lived in Toronto. Robert said the weather there was rainy. They talked for ten minutes. Before Ben left in August, grandpa gave him the radio. He also gave Ben a book about morse code with forty seven pages. Grandpa said learning morse code was important for radio operators. Ben promised to practice every day. He wanted to talk to people all around the world just like grandpa did.",
    {
        {"What town did grandpa live in?", {"Millbrook", "Oakville", "Riverside", "Lakewood"}, 0},
        {"What year was the radio from?", {"Nineteen fifty two", "Nineteen sixty two", "Nineteen seventy two", "Nineteen eighty two"}, 1},
        {"Where did they clean the radio?", {"In the attic", "In the kitchen", "In the garage", "In the basement"}, 2},
        {"What country was Robert from?", {"Mexico", "France", "Canada", "England"}, 2},
        {"How many pages was the morse code book?", {"Thirty seven", "Forty seven", "Fifty seven", "Sixty seven"}, 1}
    }
};

const StoryData story_mountain_hike PROGMEM = {
    "mountain-hike",
    "Mountain Hike",
    STORY_MEDIUM,
    215,
    "Carlos and his dad went hiking on Eagle Mountain. They started the trail at eight thirty in the morning. Carlos wore his new brown hiking boots and carried a green backpack. The backpack held three water bottles, five granola bars, and a first aid kit. The trail was steep and rocky. After hiking for one hour, they stopped at a stream. The water was crystal clear. Carlos saw four small fish swimming. They rested for fifteen minutes and ate granola bars. Further up the trail, they met a park ranger named David. David told them about a waterfall nearby. It was called Silver Falls and was only half a mile away. Carlos and his dad decided to see it. The waterfall was beautiful and thirty feet tall. The spray of water felt cool on their faces. Carlos took twelve pictures with his camera. At the top of the mountain, they could see the whole valley. There was a wooden sign that said the elevation was four thousand two hundred feet. They ate lunch on a big flat rock. Dad made turkey sandwiches at home that morning. They got back to the parking lot at three in the afternoon. Carlos slept the whole car ride home.",
    {
        {"What mountain did they hike?", {"Bear Mountain", "Eagle Mountain", "Wolf Mountain", "Hawk Mountain"}, 1},
        {"What color was Carlos's backpack?", {"Blue", "Red", "Green", "Black"}, 2},
        {"What was the ranger's name?", {"Michael", "David", "Robert", "James"}, 1},
        {"What was the waterfall called?", {"Crystal Falls", "Silver Falls", "Rainbow Falls", "Thunder Falls"}, 1},
        {"How tall was the waterfall?", {"Twenty feet", "Thirty feet", "Forty feet", "Fifty feet"}, 1}
    }
};

const StoryData story_science_fair PROGMEM = {
    "science-fair",
    "The Science Fair",
    STORY_MEDIUM,
    208,
    "Maya and her dad worked on a science fair project for two weeks. They decided to build a volcano that would really erupt. The volcano was made of clay and painted brown and gray. It stood fourteen inches tall on a wooden base. For the eruption they used baking soda and vinegar. Maya wrote a report explaining how the chemical reaction worked. The science fair was held in the school gym on March twenty third. There were forty two projects from students in grades three through five. Maya was in fourth grade. She set up her display table near the main door. A judge named Dr. Martinez came to look at her volcano. She asked Maya three questions about the experiment. Maya did the eruption for her. Red foam bubbled out of the top and ran down the sides. Dr. Martinez smiled and wrote notes on her clipboard. At the end of the fair the principal announced the winners. Maya won second place and received a blue ribbon. The first place winner was a boy named Kevin who made a solar system model. Maya was proud of her ribbon. She hung it on her bedroom wall next to her bookshelf. Dad said they would do an even better project next year.",
    {
        {"How long did they work on the project?", {"One week", "Two weeks", "Three weeks", "Four weeks"}, 1},
        {"How tall was the volcano?", {"Twelve inches", "Fourteen inches", "Sixteen inches", "Eighteen inches"}, 1},
        {"What was the date of the science fair?", {"March twentieth", "March twenty first", "March twenty second", "March twenty third"}, 3},
        {"What was the judge's name?", {"Dr. Garcia", "Dr. Martinez", "Dr. Lopez", "Dr. Rivera"}, 1},
        {"What place did Maya win?", {"First place", "Second place", "Third place", "Fourth place"}, 1}
    }
};

const StoryData story_train_journey PROGMEM = {
    "train-journey",
    "The Train Journey",
    STORY_MEDIUM,
    223,
    "Sophie and her mother took a train from Chicago to Denver. The train was called the Mountain Express. They left at six fifteen in the evening. Their seats were in car number seven. The train had big windows so they could watch the scenery. A woman in a blue uniform brought them dinner. Sophie had chicken with rice and green beans. Her mother had fish with potatoes. For dessert, they shared a piece of apple pie. The pie cost four dollars and fifty cents. The sun set over the fields and everything turned orange and pink. Sophie saw horses and cows in the distance. She counted eleven horses before it got too dark. They had sleeping cabins on the train. Each cabin had two beds stacked on top of each other. Sophie took the top bed. The sheets were white and very soft. She fell asleep listening to the train wheels clicking on the tracks. The next morning, they woke up in the mountains. Snow covered the peaks. They ate breakfast in the dining car. Sophie had pancakes with maple syrup. The train arrived in Denver at nine forty five in the morning. The whole trip took about fifteen hours. Sophies aunt was waiting for them at the station with a sign.",
    {
        {"What was the train called?", {"Silver Star", "Mountain Express", "Western Wind", "Prairie Line"}, 1},
        {"What car number were their seats in?", {"Car five", "Car six", "Car seven", "Car eight"}, 2},
        {"What did Sophie have for dinner?", {"Fish with potatoes", "Chicken with rice", "Beef with vegetables", "Pasta with sauce"}, 1},
        {"How much did the apple pie cost?", {"Three dollars", "Four dollars fifty", "Five dollars", "Six dollars"}, 1},
        {"What bed did Sophie take?", {"Bottom bed", "Top bed", "Middle bed", "Left bed"}, 1}
    }
};

const StoryData story_piano_recital PROGMEM = {
    "piano-recital",
    "The Piano Recital",
    STORY_MEDIUM,
    215,
    "Olivia had been taking piano lessons for three years. Her teacher was Mrs. Romano who lived on Cherry Lane. This Saturday was the spring recital at Harmony Music Hall. Olivia would play a piece called Moonlight Waltz. She practiced for one hour every day for a month. On the day of the recital Olivia wore a green dress with white flowers. Her grandmother came from Boston to watch. The music hall had two hundred seats and they were almost all full. Olivia was the seventh performer on the program. She walked to the black grand piano on the stage. The spotlight was very bright and warm. Her hands shook a little as she placed her fingers on the keys. Then she began to play. The music flowed smoothly. She only made one small mistake near the end. When she finished the audience clapped loudly. Her grandmother stood up and cheered. After the show Mrs. Romano gave each student a rose. Olivias rose was pink with a yellow ribbon. The family went to dinner at an Italian restaurant to celebrate. Olivia ordered spaghetti with meatballs. Grandmother gave her a gift. It was a music box that played a classical song when you opened the lid. Olivia said it was the best day of her life.",
    {
        {"How many years had Olivia been taking lessons?", {"Two years", "Three years", "Four years", "Five years"}, 1},
        {"What street did Mrs. Romano live on?", {"Oak Lane", "Maple Lane", "Cherry Lane", "Pine Lane"}, 2},
        {"What piece did Olivia play?", {"Starlight Sonata", "Moonlight Waltz", "Sunset Symphony", "Morning Melody"}, 1},
        {"What color was Olivia's dress?", {"Blue", "Pink", "Green", "Yellow"}, 2},
        {"Where did grandmother come from?", {"New York", "Boston", "Chicago", "Philadelphia"}, 1}
    }
};

const StoryData story_fishing_trip PROGMEM = {
    "fishing-trip",
    "The Fishing Trip",
    STORY_MEDIUM,
    220,
    "Tom and his father left the house at five in the morning. They were going fishing at Crystal Lake which was twenty miles away. Dad packed sandwiches and two thermoses of hot coffee in a brown cooler. Tom brought his new fishing rod that was six feet long. The rod was blue and gray with a silver reel. They arrived at the lake just as the sun was rising. The water was calm and reflected the orange sky like a mirror. They rented a small green boat with oars from the dock. The rental cost fifteen dollars for the whole day. They rowed out to the middle of the lake. Tom used worms for bait. His father used small plastic lures that looked like minnows. For the first two hours they caught nothing. Tom was getting bored. Then suddenly his rod bent sharply. He had hooked a fish. Dad helped him reel it in slowly. It was a largemouth bass weighing three pounds. Tom was so excited. By noon they had caught four more fish. The biggest one was five pounds. They took pictures with their catch. On the way home they stopped at a diner. Tom had a cheeseburger with fries. Dad had grilled fish. It was a perfect day.",
    {
        {"What time did they leave the house?", {"Four in the morning", "Five in the morning", "Six in the morning", "Seven in the morning"}, 1},
        {"What was the lake called?", {"Clear Lake", "Crystal Lake", "Silver Lake", "Mirror Lake"}, 1},
        {"How much did the boat rental cost?", {"Ten dollars", "Fifteen dollars", "Twenty dollars", "Twenty five dollars"}, 1},
        {"How long was Tom's fishing rod?", {"Five feet", "Six feet", "Seven feet", "Eight feet"}, 1},
        {"How much did the first fish weigh?", {"Two pounds", "Three pounds", "Four pounds", "Five pounds"}, 1}
    }
};

const StoryData story_zoo_adventure PROGMEM = {
    "zoo-adventure",
    "Zoo Adventure",
    STORY_MEDIUM,
    225,
    "Miss Taylor's third grade class took a field trip to Riverside Zoo. The bus left school at eight thirty and arrived at nine fifteen. There were twenty four students and three adult helpers. Everyone received a map of the zoo at the entrance. The first stop was the elephant exhibit. There were four elephants including a baby that was born in March. The baby elephant weighed five hundred pounds. Next they visited the penguin house. It was cold inside like a freezer. The class counted seventeen penguins swimming and playing. At eleven thirty they ate lunch at the zoo picnic area. Everyone brought their own food. Jake had a peanut butter sandwich but he traded half with Maria for her apple slices. After lunch they saw the lions and tigers. One tiger was sleeping on a big rock. The zookeeper said the tiger was named Rajah and was twelve years old. The last exhibit was the reptile house. Lucas was scared of the snakes at first. Then he saw a small green tree frog and thought it was cute. The gift shop was near the exit. Maya bought a stuffed elephant for nine dollars. The class got back on the bus at two. Everyone was tired but happy. Miss Taylor said it was the best field trip of the year.",
    {
        {"What time did the bus arrive at the zoo?", {"Nine o'clock", "Nine fifteen", "Nine thirty", "Nine forty five"}, 1},
        {"How many students were on the trip?", {"Twenty two", "Twenty four", "Twenty six", "Twenty eight"}, 1},
        {"How much did the baby elephant weigh?", {"Four hundred pounds", "Five hundred pounds", "Six hundred pounds", "Seven hundred pounds"}, 1},
        {"What was the tiger's name?", {"Raja", "Rajah", "Shere", "Khan"}, 1},
        {"How much did the stuffed elephant cost?", {"Seven dollars", "Eight dollars", "Nine dollars", "Ten dollars"}, 2}
    }
};

const StoryData story_camping_trip PROGMEM = {
    "camping-trip",
    "The Camping Trip",
    STORY_MEDIUM,
    128,
    "The Park family went camping at Lake Pine on Friday evening. They drove for one hour to get there. Dad set up a big blue tent near the water. Mom started a campfire using small sticks and matches. They roasted hot dogs on long metal sticks for dinner. After dinner they made smores with chocolate and marshmallows. The fire crackled and glowed orange. When it got dark they looked at the stars. Dad pointed out a group of stars called the Big Dipper. Lily counted fifteen stars before she got sleepy. They went to sleep in their sleeping bags at nine. The sleeping bags were red and very warm. An owl hooted somewhere in the trees. In the morning they ate cereal with milk from a cooler.",
    {
        {"What was the lake called?", {"Lake Oak", "Lake Pine", "Lake Birch", "Lake Maple"}, 1},
        {"How long was the drive?", {"Thirty minutes", "One hour", "Two hours", "Three hours"}, 1},
        {"What color was the tent?", {"Green", "Blue", "Orange", "Yellow"}, 1},
        {"What stars did dad point out?", {"Orion", "The Big Dipper", "The North Star", "The Little Dipper"}, 1},
        {"What color were the sleeping bags?", {"Blue", "Green", "Red", "Brown"}, 2}
    }
};

// ============================================
// HARD STORIES
// ============================================

const StoryData story_north_wind_sun PROGMEM = {
    "north-wind-sun",
    "The North Wind and the Sun",
    STORY_HARD,
    165,
    "The North Wind and the Sun had an argument about who was stronger. They saw a traveler walking on the road below wearing a thick brown coat. They agreed to have a contest. Whoever could make the traveler take off his coat would be the winner. The North Wind went first. He blew as hard as he could with icy cold gusts. The wind howled for ten minutes. But the harder he blew the tighter the traveler wrapped his coat around himself. The North Wind gave up. Then it was the Sun's turn. The Sun came out from behind the clouds and shone brightly. The air became warm and pleasant. After just five minutes the traveler unbuttoned his coat. A few minutes later he took it off completely and sat down to rest under a tree. The Sun had won the contest. This fable teaches us that gentleness and persuasion are often more effective than force and aggression.",
    {
        {"What were arguing about strength?", {"Two clouds", "North Wind and Sun", "Rain and Snow", "Day and Night"}, 1},
        {"What was the traveler wearing?", {"A thick brown coat", "A blue jacket", "A red sweater", "A green vest"}, 0},
        {"How long did the wind blow?", {"Five minutes", "Ten minutes", "Fifteen minutes", "Twenty minutes"}, 1},
        {"How long until the traveler unbuttoned?", {"Two minutes", "Five minutes", "Ten minutes", "Fifteen minutes"}, 1},
        {"What is the lesson of this fable?", {"Be strong", "Be quick", "Be gentle", "Be loud"}, 2}
    }
};

const StoryData story_town_country_mouse PROGMEM = {
    "town-country-mouse",
    "The Town Mouse and Country Mouse",
    STORY_HARD,
    195,
    "A Town Mouse visited his cousin who lived in the country. The Country Mouse was happy to see his relative and prepared a simple dinner of barley and roots. The Town Mouse looked at the plain food and said he felt sorry for his cousin. He invited the Country Mouse to come visit him in the city where there was much better food to eat. The Country Mouse agreed to visit. When they arrived at the town house the next week the Country Mouse saw a grand dining room. There were plates of cheese, bread, fruit, and honey on the big wooden table. The mice began to eat happily. But suddenly they heard a loud noise. It was the house cat coming into the room. The mice ran and hid behind the wall. When it was safe they came out again. But then they heard people walking nearby. The mice had to hide again three more times during dinner. Finally the Country Mouse said goodbye. He told his cousin that he would rather eat simple food in peace than rich food in fear. He went back home to his quiet life in the country.",
    {
        {"Who visited whom first?", {"Country visited Town", "Town visited Country", "They met halfway", "Neither visited"}, 1},
        {"What did Country Mouse serve?", {"Cheese and bread", "Barley and roots", "Fruit and honey", "Meat and vegetables"}, 1},
        {"Where was the grand dinner?", {"In a barn", "In a dining room", "In a kitchen", "In a garden"}, 1},
        {"How many times did they have to hide?", {"Two times", "Three times", "Four times", "Five times"}, 2},
        {"What did Country Mouse prefer?", {"Rich food in fear", "Simple food in peace", "City life", "Adventure"}, 1}
    }
};

const StoryData story_time_capsule PROGMEM = {
    "time-capsule",
    "The Time Capsule",
    STORY_HARD,
    188,
    "In nineteen ninety five the students at Lincoln Elementary School buried a time capsule in the playground. They put drawings, photos, a newspaper, and letters to the future inside a metal box. Mrs. Patterson the principal sealed it with red tape. Twenty five years later in twenty twenty the school decided to dig it up. The current students gathered around as workers carefully opened the old box. Inside they found a letter from a girl named Sarah who was in fifth grade. She wrote about her favorite band and the video games she played. There was a class photo showing students with big hair and bright colored clothes. The newspaper talked about events that seemed like ancient history now. One drawing showed what a student thought cars would look like in the future. Everyone laughed because the drawing showed flying cars. The original students who buried the capsule were now adults in their thirties. Some of them came back to the school for the opening ceremony. Sarah was now a teacher herself at another school. She cried happy tears when she read her old letter. The current students decided to bury a new time capsule to be opened in twenty forty five.",
    {
        {"What year was the capsule buried?", {"Nineteen ninety", "Nineteen ninety five", "Two thousand", "Two thousand five"}, 1},
        {"Who sealed the box?", {"The mayor", "Mrs. Patterson", "Mr. Johnson", "The teacher"}, 1},
        {"How many years was it buried?", {"Twenty years", "Twenty five years", "Thirty years", "Thirty five years"}, 1},
        {"What did the drawing show?", {"Flying cars", "Robots", "Spaceships", "Tall buildings"}, 0},
        {"What does Sarah do now?", {"A doctor", "A lawyer", "A teacher", "An artist"}, 2}
    }
};

const StoryData story_ocean_voyage PROGMEM = {
    "ocean-voyage",
    "Ocean Voyage",
    STORY_HARD,
    205,
    "Captain Maria Santos prepared her sailing boat for a long journey across the Atlantic Ocean. Her boat was named Blue Horizon and was thirty two feet long. She would sail from Florida to Portugal which was about four thousand miles. Maria checked everything twice. She had enough food for forty days and water for sixty days. The boat had a radio, compass, and modern navigation equipment. She left the harbor on June fifteenth at eight in the morning. Many people came to wave goodbye. The first week was calm and pleasant. Maria sailed about one hundred miles each day. She saw dolphins playing near the boat on day three. On day twelve a big storm came. The waves were fifteen feet high and the wind howled all night. Maria stayed inside the cabin and let the boat ride the waves. The storm lasted two days. After that the sea was calm again. She passed a large cargo ship on day twenty. They communicated by radio and the crew wished her luck. Maria reached Portugal on July twenty first after thirty six days at sea. Hundreds of people cheered as she sailed into the harbor. She had completed her dream journey across the ocean alone.",
    {
        {"What was the boat's name?", {"Sea Spirit", "Blue Horizon", "Ocean Star", "Wind Dancer"}, 1},
        {"How long was the boat?", {"Twenty eight feet", "Thirty feet", "Thirty two feet", "Thirty five feet"}, 2},
        {"When did she leave?", {"June tenth", "June fifteenth", "June twentieth", "July first"}, 1},
        {"How high were the storm waves?", {"Ten feet", "Fifteen feet", "Twenty feet", "Twenty five feet"}, 1},
        {"How many days did the voyage take?", {"Thirty days", "Thirty six days", "Forty days", "Forty five days"}, 1}
    }
};

// ============================================
// EXPERT STORIES
// ============================================

const StoryData story_camel_hump PROGMEM = {
    "camel-hump",
    "How the Camel Got His Hump",
    STORY_EXPERT,
    248,
    "In the beginning when the world was new and the animals were just beginning to work for Man there was a Camel who lived in the middle of a Howling Desert. He ate sticks and thorns and milkweed and prickles because he was very lazy and did not want to work. When anyone spoke to him he only said Humph and nothing more. One Monday morning the Horse came to him with a saddle on his back and said Camel come out and trot like the rest of us. Humph said the Camel and the Horse went away and told the Man. Then the Dog came with a stick in his mouth and said Camel come and fetch and carry like the rest of us. Humph said the Camel and the Dog went away and told the Man. Then the Ox came with the yoke on his neck and said Camel come and plough like the rest of us. Humph said the Camel and the Ox went away and told the Man. At the end of the day the Man called the other animals together. He said he was very sorry but the Camel who says Humph would not work so you three must work double time to make up for him. This made the Horse and Dog and Ox very angry. They held a meeting on the edge of the Desert and the Camel came along chewing milkweed and laughed at them. Then came the Djinn in charge of All Deserts rolling in a cloud of dust. The Djinn spoke to the Camel about his lazy humph and suddenly the Camel's back puffed up into a great big hump. And that is how the Camel got his Hump.",
    {
        {"Where did the Camel live?", {"By the river", "In the Howling Desert", "On a mountain", "In the forest"}, 1},
        {"What did the Camel always say?", {"No", "Humph", "Later", "Never"}, 1},
        {"Which animal came first?", {"The Dog", "The Ox", "The Horse", "The Cat"}, 2},
        {"Who made the other animals work double?", {"The Djinn", "The Man", "The Camel", "The Horse"}, 1},
        {"Who gave the Camel his hump?", {"The Man", "The Horse", "The Djinn", "The Dog"}, 2}
    }
};

const StoryData story_whale_throat PROGMEM = {
    "whale-throat",
    "How the Whale Got His Throat",
    STORY_EXPERT,
    268,
    "In the sea once upon a time there was a Whale who ate fish. He ate the starfish and the garfish and the crab and the dab and the plaice and the dace and the skate and his mate and the mackerel and the pickerel and the eel. All the fish he could find in all the sea he ate with his mouth. At last there was only one small fish left in all the sea and he was a small clever fish. He swam behind the Whale's ear to be safe. The Whale stood up on his tail and said I am hungry. The small clever fish said in a small clever voice Noble and generous Whale have you ever tasted Man. No said the Whale what is it like. Nice said the small clever fish but it is rather hard to catch. The small fish told the Whale where to find a shipwrecked Mariner floating on a raft in the middle of the sea. So the Whale swam and swam until he came to the raft where the Mariner was sitting. Then he opened his mouth wide and swallowed the Mariner and his raft. But the Mariner was also clever. He jumped and bumped and thumped and stumped and danced inside the Whale's stomach until the Whale felt very uncomfortable. The Whale said to him Come out and behave yourself. No said the Mariner take me to my home on the shore. So the Whale swam to the shore and the Mariner climbed out. But before leaving the Mariner fixed his raft in the Whale's throat so that the Whale could never again swallow anything larger than very small fish. And that is why whales today can only eat tiny things like shrimp.",
    {
        {"What did the Whale eat?", {"Only shrimp", "All the fish he could find", "Only small fish", "Plants"}, 1},
        {"Where did the small fish hide?", {"Behind the Whale's ear", "In a cave", "Under a rock", "In the sand"}, 0},
        {"What did the small fish suggest?", {"Eat seaweed", "Taste Man", "Sleep more", "Swim faster"}, 1},
        {"What did the Mariner do inside?", {"Slept quietly", "Jumped and danced", "Built a fire", "Sang songs"}, 1},
        {"Why can whales only eat small things now?", {"They got old", "The raft blocks their throat", "They lost teeth", "They are lazy"}, 1}
    }
};

// ============================================
// Story Collection Arrays
// ============================================

static const StoryData* const allStories[] PROGMEM = {
    // Tutorial
    &story_tutorial,
    // Easy
    &story_red_balloon,
    &story_grandmas_kitchen,
    &story_lost_puppy,
    &story_birthday_cake,
    &story_library_visit,
    &story_pet_shop,
    &story_dragons_treasure,
    &story_wizards_tower,
    &story_fox_grapes,
    &story_crow_pitcher,
    // Medium
    &story_old_radio,
    &story_mountain_hike,
    &story_science_fair,
    &story_train_journey,
    &story_piano_recital,
    &story_fishing_trip,
    &story_zoo_adventure,
    &story_camping_trip,
    // Hard
    &story_north_wind_sun,
    &story_town_country_mouse,
    &story_time_capsule,
    &story_ocean_voyage,
    // Expert
    &story_camel_hump,
    &story_whale_throat
};

#define TOTAL_STORY_COUNT (sizeof(allStories) / sizeof(allStories[0]))

// ============================================
// Story Access Functions
// ============================================

int getStoryCount() {
    return TOTAL_STORY_COUNT;
}

const StoryData* getStoryByIndex(int index) {
    if (index < 0 || index >= (int)TOTAL_STORY_COUNT) return NULL;
    return (const StoryData*)pgm_read_ptr(&allStories[index]);
}

int getStoryCountByDifficulty(StoryDifficulty diff) {
    int count = 0;
    for (int i = 0; i < (int)TOTAL_STORY_COUNT; i++) {
        const StoryData* story = getStoryByIndex(i);
        if (story && story->difficulty == diff) count++;
    }
    return count;
}

const StoryData* getStoryByDifficultyAndIndex(StoryDifficulty diff, int index) {
    int count = 0;
    for (int i = 0; i < (int)TOTAL_STORY_COUNT; i++) {
        const StoryData* story = getStoryByIndex(i);
        if (story && story->difficulty == diff) {
            if (count == index) return story;
            count++;
        }
    }
    return NULL;
}

// Get the index within a difficulty level for a global story index
int getStoryIndexInDifficulty(StoryDifficulty diff, int globalIndex) {
    const StoryData* targetStory = getStoryByIndex(globalIndex);
    if (!targetStory || targetStory->difficulty != diff) return -1;

    int count = 0;
    for (int i = 0; i < globalIndex; i++) {
        const StoryData* story = getStoryByIndex(i);
        if (story && story->difficulty == diff) count++;
    }
    return count;
}

// Get global index from difficulty and index within difficulty
int getGlobalStoryIndex(StoryDifficulty diff, int diffIndex) {
    int count = 0;
    for (int i = 0; i < (int)TOTAL_STORY_COUNT; i++) {
        const StoryData* story = getStoryByIndex(i);
        if (story && story->difficulty == diff) {
            if (count == diffIndex) return i;
            count++;
        }
    }
    return -1;
}

#endif // GAME_STORY_TIME_DATA_H
