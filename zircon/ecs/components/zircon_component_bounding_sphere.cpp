#include "zircon_component_bounding_sphere.h"

zircon_component_bounding_sphere::zircon_component_bounding_sphere(void) :
#ifdef KOTEK_DEBUG
	m_quality{},
#endif

	m_radius{}
{
}

zircon_component_bounding_sphere::~zircon_component_bounding_sphere(void) {}

void zircon_component_bounding_sphere::DrawImGui(
	Kotek::Core::ktkMainManager* p_main_manager) noexcept
{
	if (p_main_manager)
	{
		auto* p_wrapper_imgui = p_main_manager->Get_ImguiWrapper();

		if (p_wrapper_imgui)
		{
			if (p_wrapper_imgui->CollapsingHeader("Bounding Sphere"))
			{
#ifdef KOTEK_DEBUG
				p_wrapper_imgui->Text(
					"Built with quality: %d", this->m_quality);
#endif

				p_wrapper_imgui->Text("X: %f Y: %f Z: %f",
					this->m_center.Get_X(), this->m_center.Get_Y(),
					this->m_center.Get_Z());
				p_wrapper_imgui->Text("Radius: %f", this->m_radius);
			}
		}
	}
}

Kotek::ktk::float_t zircon_component_bounding_sphere::get_radius(
	void) const noexcept
{
	return this->m_radius;
}

void zircon_component_bounding_sphere::set_radius(
	Kotek::ktk::float_t value) noexcept
{
	this->m_radius = value;
}

const Kotek::ktk::math::vec3f_t& zircon_component_bounding_sphere::get_center(
	void) const noexcept
{
	return this->m_center;
}

void zircon_component_bounding_sphere::set_center(
	const Kotek::ktk::math::vec3f_t& point) noexcept
{
	this->m_center = point;
}

void zircon_component_bounding_sphere::include(
	const kn_kotek::kn_ktk::kn_math::vec3f_t& point) noexcept
{
	auto distance_squared = kn_kotek::kn_ktk::kn_math::distance_squared(this->m_center, point);

	if (distance_squared <= this->m_radius * this->m_radius)
		return;
	
	auto distance = std::sqrt(distance_squared);

	auto distance_sphere = distance - this->m_radius;
	auto distance_sphere_half = distance_sphere / 2.0f;

	this->m_center += (point - this->m_center) / (distance * distance_sphere_half);
	this->m_radius += distance_sphere_half;
}

struct StackElement
{
	float m_z;
	float m_radius_squared;
	kn_kotek::kn_ktk::kn_math::vec3f_t m_v;
	kn_kotek::kn_ktk::kn_math::vec3f_t m_center;
};

struct TempSphere
{
	kn_kotek::kn_ktk::list<kn_kotek::kn_ktk::kn_math::vec3f_t>::const_iterator
		m_s;
	kn_kotek::kn_ktk::vector<StackElement> m_stack;
	kn_kotek::kn_ktk::kn_math::vec3f_t m_center;
	float m_radius_squared;
};

float calculate_excess(const kn_kotek::kn_ktk::kn_math::vec3f_t& center,
	float radius_squared, const kn_kotek::kn_ktk::kn_math::vec3f_t& point)
{
	return kn_kotek::kn_ktk::kn_math::distance_squared(point, center) -
		radius_squared;
}

template <typename InputIterator, typename Type = float>
std::pair<Type, InputIterator> calculate_max_excess(
	const kn_kotek::kn_ktk::kn_math::vec3f_t& center, Type radiusSquared,
	InputIterator first, InputIterator last)
{
	Type maxExcess = std::numeric_limits<Type>::lowest();
	InputIterator result;
	for (; first != last; ++first)
	{
		const Type excess = calculate_excess(center, radiusSquared, *first);
		if (excess > maxExcess)
		{
			maxExcess = excess;
			result = first;
		}
	}
	return std::make_pair(maxExcess, result);
}

bool mb_bar(const kn_kotek::kn_ktk::kn_math::vec3f_t& point, TempSphere& data)
{
	if (data.m_stack.empty())
	{
		StackElement element;
		element.m_center = point;
		element.m_radius_squared = 0.0f;
		data.m_stack.emplace_back(std::move(element));
	}
	else
	{
		const auto stack_size = data.m_stack.size();
		StackElement current;
		const auto& prev = data.m_stack.back();

		current.m_v = point - data.m_stack[0].m_center;

		kn_kotek::kn_ktk::kn_math::vec3f_t alpha;
		for (kn_kotek::kn_ktk::uint32_t i = 1; i < stack_size; ++i)
		{
			alpha[i] = (2 / data.m_stack[i].m_z) *
				kn_kotek::kn_ktk::kn_math::dot(
					data.m_stack[i].m_v, current.m_v);
		}

		for (kn_kotek::kn_ktk::uint32_t i = 1; i < stack_size; ++i)
		{
			current.m_v -= data.m_stack[i].m_v * alpha[i];
		}

		current.m_z =
			2 * kn_kotek::kn_ktk::kn_math::dot(current.m_v, current.m_v);

		constexpr float epsilon = std::numeric_limits<float>::epsilon();

		if (current.m_z < epsilon * prev.m_radius_squared)
		{
			return false;
		}

		const float excess =
			calculate_excess(prev.m_center, prev.m_radius_squared, point);
		const float factor = excess / current.m_z;

		current.m_center = prev.m_center + current.m_v * factor;
		current.m_radius_squared = prev.m_radius_squared + factor * excess / 2;
		data.m_stack.emplace_back(std::move(current));
	}

	data.m_center = data.m_stack.back().m_center;
	data.m_radius_squared = data.m_stack.back().m_radius_squared;

	return true;
}

void move_to_front(
	kn_kotek::kn_ktk::list<kn_kotek::kn_ktk::kn_math::vec3f_t>& points,
	kn_kotek::kn_ktk::list<kn_kotek::kn_ktk::kn_math::vec3f_t>::const_iterator
		point)
{
	points.emplace_front(*point);
	points.erase(point);
}

void mtf_mb_float(
	kn_kotek::kn_ktk::list<kn_kotek::kn_ktk::kn_math::vec3f_t>& points,
	kn_kotek::kn_ktk::list<kn_kotek::kn_ktk::kn_math::vec3f_t>::const_iterator
		end_point,
	TempSphere& data)
{
	data.m_s = points.cbegin();

	if (data.m_stack.size() == 4)
	{
		return;
	}

	for (auto it = points.cbegin(); it != end_point;)
	{
		auto i = it++;

		if (calculate_excess(data.m_center, data.m_radius_squared, *i) > 0)
		{
			if (mb_bar(*i, data))
			{
				mtf_mb_float(points, i, data);
				data.m_stack.pop_back();

				if (data.m_s == i)
				{
					++data.m_s;
				}

				move_to_front(points, i);
			}
		}
	}
}

// pair = [center, radius]
std::pair<kn_kotek::kn_ktk::kn_math::vec3f_t, float> pivot_mb_float(
	kn_kotek::kn_ktk::list<kn_kotek::kn_ktk::kn_math::vec3f_t>& points)
{
	TempSphere data;
	data.m_radius_squared = std::numeric_limits<float>::lowest();
	data.m_center.Set_X(0.0f).Set_Y(0.0f).Set_Z(0.0f);

	auto iter = std::next(points.cbegin());
	mtf_mb_float(points, iter, data);
	float max_excess;
	float old_radius_squared = std::numeric_limits<float>::lowest();

	do
	{
		auto pair = calculate_max_excess(
			data.m_center, data.m_radius_squared, iter, points.cend());

		max_excess = pair.first;
		const auto& k = pair.second;

		if (max_excess > 0.0f)
		{
			iter = data.m_s;
			if (iter == k)
			{
				std::advance(iter, 1);
			}

			old_radius_squared = data.m_radius_squared;
			mb_bar(*k, data);
			mtf_mb_float(points, data.m_s, data);
			data.m_stack.pop_back();

			if (data.m_s == k)
			{
				++data.m_s;
			}

			move_to_front(points, k);
		}
	} while (max_excess > 0.0f && data.m_radius_squared > old_radius_squared);

	return {data.m_center, std::sqrt(data.m_radius_squared)};
}

zircon_component_bounding_sphere calculate_miniball_list(
	kn_kotek::kn_ktk::list<kn_kotek::kn_ktk::kn_math::vec3f_t>& points)
{
	zircon_component_bounding_sphere result;

	struct CompareByMemory
	{
		bool operator()(const kn_kotek::kn_ktk::kn_math::vec3f_t& a,
			const kn_kotek::kn_ktk::kn_math::vec3f_t& b) const
		{
			return (std::memcmp(
					   &a, &b, sizeof(kn_kotek::kn_ktk::kn_math::vec3f_t))) < 0;
		}
	};

	points.sort(CompareByMemory());
	points.unique();

	const auto& pair = pivot_mb_float(points);

	result.set_center(pair.first);
	result.set_radius(pair.second);

	return result;
}

zircon_component_bounding_sphere calculate_miniball(
	const kn_kotek::kn_ktk::vector<kn_kotek::kn_ktk::kn_math::vec3f_t>&
		geometry)
{
	zircon_component_bounding_sphere result;

	kn_kotek::kn_ktk::list<kn_kotek::kn_ktk::kn_math::vec3f_t> points;

	for (const auto& point : geometry)
	{
		points.emplace_back(point);
	}

	result = calculate_miniball_list(points);

	return result;
}

void project_to_normal(
	kn_kotek::kn_ktk::vector<kn_kotek::kn_ktk::kn_math::vec3f_t>::const_iterator
		begin,
	const kn_kotek::kn_ktk::vector<
		kn_kotek::kn_ktk::kn_math::vec3f_t>::const_iterator& end,
	kn_kotek::kn_ktk::vector<kn_kotek::kn_ktk::vector<
		kn_kotek::kn_ktk::kn_math::vec3f_t>::const_iterator>& result,
	int nx, int ny, int nz)
{
	float min_value = std::numeric_limits<float>::max();
	float max_value = std::numeric_limits<float>::lowest();

	auto min_point = end;
	auto max_point = end;

	for (; begin != end; ++begin)
	{
		const auto& point = *begin;
		const auto projection = static_cast<float>(nx) * point.Get_X() +
			static_cast<float>(ny) * point.Get_Y() +
			static_cast<float>(nz) * point.Get_Z();

		if (projection < min_value)
		{
			min_value = projection;
			min_point = begin;
		}

		if (projection > max_value)
		{
			max_value = projection;
			max_point = begin;
		}
	}

	result.emplace_back(min_point);
	result.emplace_back(max_point);
}

kn_kotek::kn_ktk::list<kn_kotek::kn_ktk::kn_math::vec3f_t> find_extremal_points(
	const kn_kotek::kn_ktk::vector<kn_kotek::kn_ktk::kn_math::vec3f_t>& points,
	int num_normals)
{
	kn_kotek::kn_ktk::list<kn_kotek::kn_ktk::kn_math::vec3f_t> result;

	if (!points.empty())
	{
		kn_kotek::kn_ktk::vector<kn_kotek::kn_ktk::vector<
			kn_kotek::kn_ktk::kn_math::vec3f_t>::const_iterator>
			indexes;
		indexes.reserve(2 * num_normals);

		project_to_normal(points.cbegin(), points.cend(), indexes, 1, 0, 0);
		project_to_normal(points.cbegin(), points.cend(), indexes, 0, 1, 0);
		project_to_normal(points.cbegin(), points.cend(), indexes, 0, 0, 1);

		if (num_normals > 3)
		{
			project_to_normal(points.cbegin(), points.cend(), indexes, 1, 1, 1);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 1, 1, -1);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 1, -1, 1);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 1, -1, -1);
		}

		if (num_normals > 7)
		{
			project_to_normal(points.cbegin(), points.cend(), indexes, 1, 1, 0);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 1, -1, 0);
			project_to_normal(points.cbegin(), points.cend(), indexes, 1, 0, 1);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 1, 0, -1);
			project_to_normal(points.cbegin(), points.cend(), indexes, 0, 1, 1);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 0, 1, -1);
		}

		if (num_normals > 13)
		{
			project_to_normal(points.cbegin(), points.cend(), indexes, 0, 1, 2);
			project_to_normal(points.cbegin(), points.cend(), indexes, 0, 2, 1);
			project_to_normal(points.cbegin(), points.cend(), indexes, 1, 0, 2);
			project_to_normal(points.cbegin(), points.cend(), indexes, 2, 0, 1);
			project_to_normal(points.cbegin(), points.cend(), indexes, 1, 2, 0);
			project_to_normal(points.cbegin(), points.cend(), indexes, 2, 1, 0);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 0, 1, -2);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 0, 2, -1);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 1, 0, -2);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 2, 0, -1);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 1, -2, 0);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 2, -1, 0);

			project_to_normal(points.cbegin(), points.cend(), indexes, 1, 1, 2);
			project_to_normal(points.cbegin(), points.cend(), indexes, 2, 1, 1);
			project_to_normal(points.cbegin(), points.cend(), indexes, 1, 2, 1);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 1, -1, 2);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 1, 1, -2);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 1, -1, -2);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 2, -1, 1);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 2, 1, -1);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 2, -1, -1);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 1, -2, 1);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 1, 2, -1);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 1, -2, -1);

			project_to_normal(points.cbegin(), points.cend(), indexes, 2, 2, 1);
			project_to_normal(points.cbegin(), points.cend(), indexes, 1, 2, 2);
			project_to_normal(points.cbegin(), points.cend(), indexes, 2, 1, 2);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 2, -2, 1);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 2, 2, -1);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 2, -2, -1);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 1, -2, 2);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 1, 2, -2);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 1, -2, -2);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 2, -1, 2);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 2, 1, -2);
			project_to_normal(
				points.cbegin(), points.cend(), indexes, 2, -1, -2);
		}

		std::sort(indexes.begin(), indexes.end());
		indexes.erase(
			std::unique(indexes.begin(), indexes.end()), indexes.end());

		for (const auto& point : indexes)
		{
			result.emplace_back(*point);
		}
	}

	return result;
}

zircon_component_bounding_sphere calculate_optimal_sphere(
	const kn_kotek::kn_ktk::vector<kn_kotek::kn_ktk::kn_math::vec3f_t>&
		geometry,
	int precision)
{
	zircon_component_bounding_sphere result;

	KOTEK_ASSERT(
		geometry.empty() == false, "you can't pass empty geometry here!");
	KOTEK_ASSERT(precision > 0, "you must specify precision higher than 0");

	auto size = geometry.size();

	if (size > 2 * precision)
	{
		auto extremal_points = find_extremal_points(geometry, precision);
		result = calculate_miniball_list(extremal_points);

		for (const auto& point : geometry)
		{
			result.include(point);
		}
	}
	else
	{
		result = calculate_miniball(geometry);
	}

	return result;
}

// TODO: provide for double precision pipeline
zircon_component_bounding_sphere zircon_calculate_bounding_sphere(
	const kn_kotek::kn_ktk::vector<kn_kotek::kn_ktk::kn_math::vec3f_t>&
		geometry,
	int precision)
{
	return calculate_optimal_sphere(geometry, precision);
}
