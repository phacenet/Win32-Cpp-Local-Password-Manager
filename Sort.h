#ifndef SORT_H
#define SORT_H

#include <utility> //iter_swap

template<typename RandomIt>
RandomIt median_of_three(RandomIt first, RandomIt mid, RandomIt last)
{
	//avoids needing to include algorithm for std::min and std::max
	if((*first <= *mid && *mid <= *last) || (*last <= *mid && *mid <= *first))
		return mid;
	else if((*mid <= *first && *first <= *last) || (*last <= *first && *first <= *mid))
		return first;
	else
		return last;
}

template <typename RandomIt>
void insertion_sort(RandomIt first, RandomIt last)
{
	//empty range
	if (first == last)
		return;

	for (RandomIt i = first + 1; i < last; ++i)
	{
		auto key = std::move(*i); //create the hole at i
		RandomIt j = i; 

		while (j > first && key < *(j - 1))
		{
			*j = std::move(*(j - 1)); //shift right
			--j;
		}
		*j = std::move(key); //fill the hole
	}
	// initial array
	// {8, 2, 4, 9, 1, 7}
	/*
	
	i = 1
	key = 2
	j = 1

	while (j > 0 && key < a[j - 1])
	while (1 > 0 && 2 < 8)  -> true
	a[1] = a[0]   -> {8, 8, 4, 9, 1, 7}
	j = 0
	while (0 > 0 && ...) -> false, stop
	a[0] = key -> {2, 8, 4, 9, 1, 7}

	i = 2
	key = 4
	j = 2

	while (2 > 0 && 4 < 8) -> true

	a[2] = a[1] -> {2, 8, 8, 9, 1, 7}
	j = 1

	while (1 > 0 && 4 < 2) -> false, stop

	a[1] = key -> {2, 4, 8, 9, 1, 7}
	*/
}

template <typename RandomIt>
void Sort(RandomIt first, RandomIt last)
{
	//no sorting for 0 or 1 elements
	if (last - first <= 1) return;

	std::size_t n = static_cast<std::size_t>(last - first);
	constexpr const std::size_t cutoff = 16;

	if (n <= cutoff)
	{
		insertion_sort(first, last);
		return;
	}

	//Pivot iterator (midpoint for now)
	RandomIt mid = first + (last - first) / 2;

	//median of 3
	RandomIt p = median_of_three(first, mid, last - 1); //last - 1 for past the end of range
	auto pivot = *p; //value since first, last can change

	RandomIt lt = first;
	RandomIt i = first;
	RandomIt gt = last;


	while(i < gt)
	{
		if (*i < pivot) //
		{
			std::iter_swap(i, lt);
			++lt;
			++i;
		}
		else if (*i > pivot)
		{
			--gt; //make gt point to valid element
			std::iter_swap(i, gt); //swapped in-element at i is unclassified
			//DONT increment i, so that the last-n element can be compared as well to ith element
		}
		else
			++i; //equal
	}

	/*
	{5,1,2,4,3} pivot = 2

	i = 0, *i = 5: 5>2, case B -> --gt -> gt=4, swap i(0) gt(4) -> {3,1,2,4,5} 
	i = 0, *i = 3: 3>2, case B -> --gt -> gt=3, swap i(0) gt(3) -> {4,1,2,3,5}
	i = 0, *i = 4: 4>2, case B -> --gt -> gt=2, swap i(0) gt(2) -> {2,1,3,4,5}
	i = 0, *i = 2: 2==2, case C -> ++i
	i = 1, *i = 1: 1<2, case A -> swap i(1) lt(0) -> {1,2,3,4,5}
	i = 2, *i = 3: !(i<gt), ends loop
	
	*/

	//not guaranteed to be sorted by this point, so sort the regions recursively. region 1: elements < pivot. region 2: elements > pivot

	//Tail recursion optimization: recurse on smaller partition first. {first, x, lt, pivot, gt, x, last} 
	if (lt - first < last - gt)
	{
		Sort(first, lt); //smaller side
		Sort(gt, last); //larger side
	}
	else
	{
		Sort(gt, last); //smaller side
		Sort(first, lt); //larger side
	}
}


#endif