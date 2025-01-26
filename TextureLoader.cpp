#include "TextureLoader.h"
#include <execution>
#include "stb_image.h"

TextureLoader* TextureLoader::instance = nullptr;

ogl::Texture2D* TextureLoader::Load(const std::string& path, void* data, size_t size,bool srgb, bool flip, bool bindless)
{
	if (instance == nullptr)
	{
		instance = new TextureLoader;
	}

	for (const auto& p : instance->promises)
	{
		const auto& [_path, _data, _size, _free_data, _srgb, _flip, _bindless, _index] = p;
		if (path == _path && data == _data && size == _size && srgb == _srgb &&  flip == _flip && bindless == _bindless)
		{
			printf("Re-Using Texture %s\n", path.c_str());
			return instance->textures[_index];
		}
	}

	// if the data is owned by the caller, we don't free it
	instance->promises.push_back({ path , data, size, data == nullptr, srgb, flip, bindless, instance->index++ });
	instance->textures.push_back(new ogl::Texture2D{});
	return instance->textures.back();
}

struct PromisedTexture
{
	std::string path;
	unsigned char* data;
	int data_size;
	int width, height, channels;
	bool bindless;
	bool srgb;
};

void TextureLoader::LoadPromisedTextures()
{
	if (instance == nullptr)
	{
		instance = new TextureLoader;
		return;
	}

	if (promises.empty()) return;

	auto promisedTextures = new PromisedTexture[promises.size()];
	memset(promisedTextures, 0, sizeof(PromisedTexture) * promises.size());

	for (size_t i = 0; i < promises.size(); i++)
	{
		const auto& [path, data, size, free_data, srgb, flip, bindless, index] = promises[i];

		if (data != nullptr && size > 0)
		{
			promisedTextures[i].data = (unsigned char*)data;
			promisedTextures[i].data_size = int(size);
			promisedTextures[i].bindless = bindless;
			promisedTextures[i].srgb = srgb;
			continue;
		}

		FILE* file;
		fopen_s(&file, path.c_str(), "rb");
		if (file) {
			fseek(file, 0, SEEK_END);
			auto size = ftell(file);
			rewind(file);
			auto buffer = new unsigned char[size];
			if (buffer) {
				fread_s(buffer, size, 1, size, file);
				promisedTextures[i].data = buffer;
				promisedTextures[i].data_size = int(size);
				promisedTextures[i].bindless = bindless;
				promisedTextures[i].srgb = srgb;
			}
		}
		else
		{
			printf("Failed to load texture %s\n", path.c_str());
		}
	}

	std::for_each(std::execution::par_unseq, promises.begin(), promises.end(),
		[&](std::tuple<std::string, void*, size_t, bool, bool, bool, bool, int>& promise) {
			const auto& [path, data, size, free_data, srgb, flip, bindless, index] = promise;
			auto& p = promisedTextures[index];
			if (p.data != nullptr)
			{
				stbi_set_flip_vertically_on_load(flip);
				auto buffer = stbi_load_from_memory(p.data, p.data_size, &p.width, &p.height, &p.channels, 0);
				if (free_data) {
					delete[] p.data;
				}

				p.data = buffer;
				p.data_size = -1;
			}
		});

	for (int i = 0; i < promises.size(); i++)
	{
		auto& p = promisedTextures[i];
		if (p.data != nullptr)
		{
			auto texture = ogl::create_texture_from_bytes(p.data, p.width, p.height, p.channels, p.srgb);
			textures[i]->id = texture.id;
			stbi_image_free(p.data);
		}
	}

	delete[] promisedTextures;

	index = 0;
	promises.clear();
	textures.clear();
}