#pragma once

#include <vk_types.h>

class GfxDevice;

enum ImageType {
	T_UNKNOWN,
	T_2D,
	T_CUBEMAP,
};

class GpuImage {
public:
	using PTR = std::shared_ptr<GpuImage>;

	GpuImage( ) = delete;
	GpuImage( GfxDevice* gfx, const std::string& name, void* data, VkExtent3D extent, VkFormat format, ImageType image_type = T_2D, VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT, bool generate_mipmaps = false );
	GpuImage( GfxDevice* gfx, const std::string& name, VkExtent3D extent, VkFormat format, ImageType image_type = T_2D, VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT, bool generate_mipmaps = false );
	~GpuImage( );

	// delete copy constructors
	GpuImage( const GpuImage& ) = delete;
	GpuImage& operator=( const GpuImage& ) = delete;

	// allow move constructors
	GpuImage( GpuImage&& other ) noexcept
		: gfx( std::exchange( other.gfx, nullptr ) )
		, id( std::exchange( other.id, -1 ) )
		, type( std::exchange( other.type, T_UNKNOWN ) )
		, image( std::exchange( other.image, VK_NULL_HANDLE ) )
		, view( std::exchange( other.view, VK_NULL_HANDLE ) )
		, extent( std::exchange( other.extent, {} ) )
		, format( std::exchange( other.format, {} ) )
		, usage( std::exchange( other.usage, {} ) )
		, allocation( std::exchange( other.allocation, {} ) )
		, mipmapped( std::exchange( other.mipmapped, false ) )
		, mip_views( std::move( other.mip_views ) )
		, name( std::move( other.name ) ) {}
	GpuImage& operator=( GpuImage&& other ) noexcept {
		if ( this != &other ) {
			gfx = std::exchange( other.gfx, nullptr );
			id = std::exchange( other.id, -1 );
			type = std::exchange( other.type, T_UNKNOWN );
			image = std::exchange( other.image, VK_NULL_HANDLE );
			view = std::exchange( other.view, VK_NULL_HANDLE );
			extent = std::exchange( other.extent, {} );
			format = std::exchange( other.format, {} );
			usage = std::exchange( other.usage, {} );
			allocation = std::exchange( other.allocation, {} );
			mipmapped = std::exchange( other.mipmapped, false );
			mip_views = std::move( other.mip_views );
			name = std::move( other.name );
		}
		return *this;
	}

	void TransitionLayout( VkCommandBuffer cmd, VkImageLayout current_layout, VkImageLayout new_layout, bool depth = false ) const;
	void GenerateMipmaps( VkCommandBuffer cmd ) const;

	ImageID GetId( ) const { return id; }
	void SetId( ImageID new_id ) { id = new_id; }
	ImageType GetType( ) const { return type; }
	VkImage GetImage( ) const { return image; }
	VkImageView GetBaseView( ) const { return view; }
	VkExtent3D GetExtent( ) const { return extent; }
	VkFormat GetFormat( ) const { return format; }
	VkImageUsageFlags GetUsage( ) const { return usage; }
	VmaAllocation GetAllocation( ) const { return allocation; }
	bool IsMipmapped( ) const { return mipmapped; }
	const VkImageView GetMipView( size_t mip_level ) const { return mip_views.at( mip_level ); }
	const std::vector<VkImageView>& GetMipViews( ) { return mip_views; }
	const std::string& GetName( ) const { return name; }

private:
	void _SetDebugName( const std::string& name );

	void _ActuallyCreateEmptyImage2D( );
	void _ActuallyCreateImage2DFromData( void* data );
	void _ActuallyCreateCubemapFromData( void* data );
	void _ActuallyCreateEmptyCubemap( );

private:
	GfxDevice* gfx;

	ImageID id = -1;

	ImageType type = T_UNKNOWN;

	VkImage image = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	VkExtent3D extent = {};
	VkFormat format = {};
	VkImageUsageFlags usage = {};

	VmaAllocation allocation = {};

	bool mipmapped = false;
	std::vector<VkImageView> mip_views;

	std::string name;
};

namespace Image {
	size_t CalculateSize( VkExtent3D extent, VkFormat format );

	void Allocate2D( VmaAllocator allocator, VkFormat format, VkExtent3D extent, VkImageUsageFlags usage, uint32_t mip_levels, VkImage* image, VmaAllocation* allocation );
	void AllocateCubemap( VmaAllocator allocator, VkFormat format, VkExtent3D extent, VkImageUsageFlags usage, uint32_t mip_levels, VkImage* image, VmaAllocation* allocation );

	VkImageView CreateView2D( VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, uint32_t mip_level = 0 );
	VkImageView CreateViewCubemap( VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, uint32_t mip_level = 0 );

	void TransitionLayout( VkCommandBuffer cmd, VkImage image, VkImageLayout current_layout, VkImageLayout new_layout, bool depth = false );

	// expects image to be in layout VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	// will leave image in layout VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
	void GenerateMipmaps( VkCommandBuffer cmd, VkImage image, VkExtent2D image_size );

	// expects image to be in layout VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL 
	void CopyFromBuffer( VkCommandBuffer cmd, VkBuffer buffer, VkImage image, VkExtent3D extent, VkImageAspectFlags aspect, VkDeviceSize offset = 0, uint32_t face = 0 );
}