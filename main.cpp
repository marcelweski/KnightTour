#include <iostream>
#include <stack>
#include <cassert>
#include <chrono>
#include <functional>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <array>
#include <future>
#include <random>
#include <sstream>

#define N 8

#define IS_VALID(x, y) (x < N && y < N && checkerboard[x][y] == 0)

#define MOVES_COUNT 8
#define MAX_TRIES 1000000000
#define MAX_THREAD_COUNT 8

struct Vec2i
{
	int32_t x, y;

	bool operator==(const Vec2i& other) const
	{
		return x == other.x && y == other.y;
	}
};

struct StackEntry
{
	uint32_t x, y;
	uint32_t nextMoveIdx;
};

struct BestScore
{
	uint64_t tries;
	uint32_t combIdx;

	BestScore()
	{
		reset();
	}

	void reset()
	{
		tries = UINT64_MAX;
		combIdx = UINT32_MAX;
	}
};
BestScore bestScore;

std::atomic<uint32_t> currentThreadCount = 0;
std::condition_variable cv;
std::mutex m;

void printCheckerboard(const uint32_t checkerboard[N][N])
{
	for (size_t x = 0; x < N; x++)
	{
		for (size_t y = 0; y < N; y++)
		{
			printf("%2i ", checkerboard[y][x]);
		}
		putchar('\n');
	}
	putchar('\n');
}

/*
 * Recursive
 */
uint32_t checkerboard[N][N] = { { 0 } };
uint64_t tries = 0;

bool knightTourR(uint32_t x, uint32_t y, uint32_t num = 1)
{
	if (num > N*N)
		return true;

	tries++;

	if (IS_VALID(x, y))
	{
		checkerboard[x][y] = num;

		if (knightTourR(x - 2, y + 1, num + 1) ||
			knightTourR(x - 1, y + 2, num + 1) ||
			knightTourR(x + 1, y + 2, num + 1) ||
			knightTourR(x + 2, y + 1, num + 1) ||
			knightTourR(x + 2, y - 1, num + 1) ||
			knightTourR(x + 1, y - 2, num + 1) ||
			knightTourR(x - 1, y - 2, num + 1) ||
		    knightTourR(x - 2, y - 1, num + 1))
		{
			return true;
		}

		checkerboard[x][y] = 0;
	}

	return false;
}

namespace estd
{
	template <typename T>
	class vector : public std::vector<T>
	{
	public:
		vector() : std::vector<T>() {}
		vector(const std::initializer_list<T>& l) : std::vector<T>(l)
		{

		}

		bool contains(const T& i)
		{
			return std::find(this->begin(), this->end(), i) != this->end();
		}

		void shuffle()
		{
			const unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
			std::shuffle(this->begin(), this->end(), std::default_random_engine(seed));
		}

		void changeCombination(uint32_t n)
		{
			uint32_t i = 0;
			while (n --> 0)
			{
				std::iter_swap(this->begin() + (i % this->size()), this->begin() + (i + 1) % this->size());
				i++;
			}
		}
	};

	template <typename T>
	std::string join(const std::string& seperator, const estd::vector<T>& v)
	{
		std::stringstream s;
		for (size_t i = 0; i < v.size(); i++)
		{
			s << std::to_string(v.at(i));
			if (i != v.size() - 1)
				s << seperator;
		}
		return s.str();
	}
}

namespace math
{
	uint32_t factorial(uint32_t x)
	{
		uint32_t r = 1;
		while (x > 1)
			r *= x--;
		return r;
	}
}

/*
 * Iterative
 */
std::tuple<bool, uint64_t> knightTourI(uint32_t x, uint32_t y, const uint32_t combIdx)
{
	uint32_t checkerboard[N][N] = { { 0 } };

	assert(IS_VALID(x, y));
	
	StackEntry stack[1 + N * N];
	StackEntry* s = stack;
	uint64_t tries = 0;
	
#define STACK_PUSH(x, y) s++; s->x = x; s->y = y; s->nextMoveIdx = 0;
#define STACK_SIZE() static_cast<uint32_t>(s - stack)
#define STACK_ISEMPTY() (s == stack)

	STACK_PUSH(x, y);

	estd::vector<Vec2i> moves = {
		{ -2, +1 },
		{ -1, +2 },
		{ +1, +2 },
		{ +2, +1 },
		{ +2, -1 },
		{ +1, -2 },
		{ -1, -2 },
		{ -2, -1 }
	};
	moves.changeCombination(combIdx);
	
	checkerboard[x][y] = STACK_SIZE();

	while (!STACK_ISEMPTY() && STACK_SIZE() < N * N)
	{
		StackEntry* e = s;

		uint32_t i = e->nextMoveIdx;
		while (i < MOVES_COUNT)
		{
			x = e->x + moves[i].x; y = e->y + moves[i].y;

			if (++tries >= MAX_TRIES)
				return std::make_tuple(false, tries);

			if (IS_VALID(x, y))
			{
				e->nextMoveIdx = i + 1;

				STACK_PUSH(x, y);
				
				checkerboard[x][y] = STACK_SIZE();
				break;
			}

			i++;
		}

		if (i >= MOVES_COUNT)
		{
			checkerboard[e->x][e->y] = 0;
			s--;
		}
	}

	return std::make_tuple(!STACK_ISEMPTY(), tries);
}

void run(const uint32_t x, const uint32_t y, const uint32_t combIdx)
{
	const auto start = std::chrono::high_resolution_clock::now();

	auto [foundSolution, tries] = knightTourI(x, y, combIdx);

	const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();

	printf("%i [%i, %i, %5i]: %lld tries in %lld ms\n", foundSolution, x, y, combIdx, tries, diff);

	std::unique_lock<std::mutex> lock(m);
	if (foundSolution && tries <= bestScore.tries)
	{
		// printf("%i [%i, %i, %5i]: %lld tries in %lld ms\n", foundSolution, x, y, combIdx, tries, diff);

		if (tries < bestScore.tries)
		{
			bestScore.tries = tries;
			bestScore.combIdx = combIdx;
		}
	}
	--currentThreadCount;
	cv.notify_all();
}

void tryCombinations(const uint32_t x, const uint32_t y, const uint32_t combinations)
{
	estd::vector<std::future<void>> futures;
	
	estd::vector<uint32_t> combIndices;
	for (uint32_t i = 0; i < combinations; i++)
		combIndices.push_back(i);
	combIndices.shuffle();

	for (uint32_t i = 0; i < combinations; i++)
	{
		++currentThreadCount;
		futures.push_back(std::async(std::launch::async, run, x, y, combIndices.at(i)));

		while (currentThreadCount == MAX_THREAD_COUNT || (i == combinations - 1 && currentThreadCount > 0))
		{
			std::unique_lock<std::mutex> lock(m);
			cv.wait(lock);
		}
	}

	printf("bestScore [%i, %i]: tries %11llu, combIdx: %5i\n", x, y, bestScore.tries, bestScore.combIdx);
	bestScore.reset();
}

int main(int argc, char** argv)
{
	std::ios::sync_with_stdio(true);
	
	/*
	 * bestScore [0, 0]: tries    25936253, combIdx:     0
	 * bestScore [0, 1]: tries     1857571, combIdx:     5
	 * bestScore [0, 2]: tries     1341912, combIdx:     4
	 * bestScore [0, 3]: tries     1992689, combIdx:     3
	 * bestScore [0, 4]: tries     4089934, combIdx:    10
	 * bestScore [0, 5]: tries     6476610, combIdx:     5
	 * bestScore [0, 6]: tries     2442574, combIdx:     4
	 * bestScore [0, 7]: tries      550520, combIdx:    13
	 *
	 * bestScore [1, 0]: tries    23818282, combIdx:    13
	 * bestScore [1, 1]: tries    13749201, combIdx:    40
	 * bestScore [1, 2]: tries   18446744073709551615, combIdx:    -1
	 * bestScore [1, 3]: tries    27014430, combIdx:     4
	 * bestScore [1, 4]: tries     5798643, combIdx:    43
	 * bestScore [1, 5]: tries   18446744073709551615, combIdx:    -1
	 * bestScore [1, 6]: tries      202251, combIdx:    17
	 * bestScore [1, 7]: tries      739901, combIdx:    10
	 *
	 * bestScore [2, 0]: tries    33382001, combIdx:    11
	 * bestScore [2, 1]: tries   184137430, combIdx:     5
	 * bestScore [2, 2]: tries     1826622, combIdx:    40
	 * bestScore [2, 3]: tries     4911387, combIdx:    23
	 * bestScore [2, 4]: tries   868191835, combIdx:    14
	 * bestScore [2, 5]: tries    85167560, combIdx:    38
	 * bestScore [2, 6]: tries     2973094, combIdx:    40
	 * bestScore [2, 7]: tries     4574810, combIdx:    23
	 *
	 * bestScore [3, 0]: tries      330579, combIdx:    12
	 * bestScore [3, 1]: tries  1102959043, combIdx:     4
	 * bestScore [3, 2]: tries    22091830, combIdx:    29
	 * bestScore [3, 3]: tries     8077893, combIdx:    30
	 * bestScore [3, 4]: tries     9221801, combIdx:    13
	 * bestScore [3, 5]: tries      222690, combIdx:    23
	 * bestScore [3, 6]: tries     1754014, combIdx:     5
	 * bestScore [3, 7]: tries      155327, combIdx:    22
	 *
	 * bestScore [4, 0]: tries     5440082, combIdx:    37
	 * bestScore [4, 1]: tries    15361875, combIdx:    37
	 * bestScore [4, 2]: tries     7946040, combIdx:     8
	 * bestScore [4, 3]: tries    38831558, combIdx:    13
	 * bestScore [4, 4]: tries    11345745, combIdx:    31
	 * bestScore [4, 5]: tries     1100437, combIdx:    40
	 * bestScore [4, 6]: tries    10202818, combIdx:    36
	 * bestScore [4, 7]: tries    34594357, combIdx:     3
	 *
	 * bestScore [5, 0]: tries  4989200462, combIdx:    53
	 * bestScore [5, 1]: tries  18446744073709551615, combIdx:    -1
	 * bestScore [5, 2]: tries    19008057, combIdx:    48
	 * bestScore [5, 3]: tries     4997172, combIdx:    37
	 * bestScore [5, 4]: tries    18008013, combIdx:    41
	 * bestScore [5, 5]: tries     5342203, combIdx:     4
	 * bestScore [5, 6]: tries     1102999, combIdx:    22
	 * bestScore [5, 7]: tries   105553780, combIdx:     4
	 * 
	 * bestScore [6, 0]: tries      665601, combIdx:    40
	 * bestScore [6, 1]: tries     9601238, combIdx:    41
	 * bestScore [6, 2]: tries     1096204, combIdx:    15
	 * bestScore [6, 3]: tries    29895762, combIdx:    13
	 * bestScore [6, 4]: tries      708838, combIdx:     9
	 * bestScore [6, 5]: tries     1440387, combIdx:    37
	 * bestScore [6, 6]: tries    10026458, combIdx:    13
	 * bestScore [6, 7]: tries     4559190, combIdx:    23
	 *
	 * bestScore [7, 0]: tries      626251, combIdx:    15
	 * bestScore [7, 1]: tries      204684, combIdx:     9
	 * bestScore [7, 2]: tries   509907482, combIdx:    13
	 * bestScore [7, 3]: tries      273390, combIdx:    39
	 * bestScore [7, 4]: tries    34594353, combIdx:     3
	 * bestScore [7, 5]: tries    32478483, combIdx:    11
	 * bestScore [7, 6]: tries    13846779, combIdx:    11
	 * bestScore [7, 7]: tries      105712, combIdx:    39
	 */
	
	// const uint32_t combinations = math::factorial(MOVES_COUNT);

	// tryCombinations(1, 2, combinations);

	const uint32_t x = 0, y = 7;
	
	const bool found = knightTourR(x, y);
	if (found)
		printf("found a solution at %i, %i\n", x, y);
	
	printCheckerboard(checkerboard);
	
	// estd::vector<int> v = { 1, 2, 3, 4 };
	// v.changeCombination(24);

	// std::cout << estd::join(", ", v) << std::endl;
	
	getchar();
	return 0;
}
