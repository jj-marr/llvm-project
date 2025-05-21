======================
Template Caching
======================

.. contents::
   :local:

Introduction
============

The Template Caching system in Clang provides a mechanism to cache C++ template instantiations, reducing compilation time for template-heavy codebases. When enabled, this feature stores serialized template instantiations in a cache and reuses them in subsequent compilations, avoiding redundant instantiations of the same templates.

User Documentation
=================

Overview
--------

C++ templates are a powerful feature that enables generic programming, but they can significantly impact compilation time. Each template instantiation requires the compiler to generate new code, which can be computationally expensive, especially in large codebases with heavy template usage.

The template caching feature addresses this issue by:

1. Storing serialized template instantiations in a cache (both in-memory and on disk)
2. Reusing these instantiations when the same template is instantiated with the same arguments
3. Reducing redundant work across multiple translation units and compilation sessions

Benefits
--------

* **Faster compilation times**: Reusing cached template instantiations can significantly reduce compilation time, especially for template-heavy codebases.
* **Reduced memory usage**: By sharing template instantiations across translation units, the overall memory footprint of the compilation process can be reduced.
* **Improved incremental builds**: When making changes to non-template code, previously cached template instantiations can be reused, making incremental builds faster.

Enabling Template Caching
-------------------------

Template caching can be enabled using the following command-line options:

.. code-block:: bash

   clang++ -ftemplate-caching source.cpp

Additional configuration options:

.. code-block:: bash

   # Specify a custom cache directory
   clang++ -ftemplate-caching -ftemplate-cache-dir=/path/to/cache source.cpp

   # Specify a custom prefix for cache files
   clang++ -ftemplate-caching -ftemplate-cache-prefix=myproject_ source.cpp

Configuration Options
--------------------

+-----------------------------+------------------------------------------+----------------------------------+
| Option                      | Description                              | Default Value                    |
+=============================+==========================================+==================================+
| ``-ftemplate-caching``      | Enable template caching                  | Disabled                         |
+-----------------------------+------------------------------------------+----------------------------------+
| ``-ftemplate-cache-dir``    | Directory to store cache files           | ``.template-cache``              |
+-----------------------------+------------------------------------------+----------------------------------+
| ``-ftemplate-cache-prefix`` | Prefix for cache files                   | ``template-``                    |
+-----------------------------+------------------------------------------+----------------------------------+
| ``-fno-template-caching``   | Disable template caching                 | -                                |
+-----------------------------+------------------------------------------+----------------------------------+

Best Practices
-------------

1. **Enable for template-heavy codebases**: Template caching is most beneficial for codebases that make heavy use of templates, such as those using the Standard Template Library (STL) extensively.

2. **Shared cache for team environments**: In team environments, consider setting up a shared template cache directory that all developers can access. This can be particularly beneficial for continuous integration systems.

3. **Clear the cache when upgrading compilers**: The cache format may change between compiler versions. It's recommended to clear the cache when upgrading to a new version of Clang.

4. **Monitor cache size**: Over time, the template cache can grow large. Periodically clean old or unused cache entries to manage disk space.

Troubleshooting
--------------

Common issues and their solutions:

1. **Cache directory permissions**: Ensure that the cache directory is writable by the user running the compiler. If using a shared cache, ensure appropriate permissions are set.

2. **Cache corruption**: If you encounter unexpected compilation errors after enabling template caching, try clearing the cache directory and recompiling.

3. **Disk space issues**: The template cache can grow large over time. If you're running low on disk space, consider clearing the cache or moving it to a location with more available space.

4. **Unexpected behavior after code changes**: If you've made changes to template code but the behavior doesn't change as expected, try clearing the cache to ensure the templates are reinstantiated.

To clear the cache, simply remove the cache directory:

.. code-block:: bash

   rm -rf .template-cache  # or your custom cache directory

Developer Documentation
======================

Architecture Overview
--------------------

The template caching system is part of the Cross Translation Unit (CTU) functionality in Clang. It consists of several key components:

1. **TemplateInstantiationCache**: The main class that manages the cache, providing methods to store and retrieve template instantiations.

2. **TemplateInstantiationKey**: Represents a key for template instantiation cache lookup, created from a template declaration and its arguments.

3. **SerializedTemplateInstantiation**: Represents a serialized template instantiation stored in the cache.

4. **CrossTranslationUnitContext**: Manages the template cache and provides an interface for the rest of Clang to interact with it.

The system supports caching of three types of template instantiations:
- Class template specializations
- Function template instantiations
- Variable template specializations

Key Classes and Responsibilities
-------------------------------

**TemplateInstantiationCache**

The central class that manages the template cache. It provides methods to:
- Store template instantiations in the cache
- Look up template instantiations in the cache
- Configure the cache (directory, prefix, enable/disable)

**TemplateInstantiationKey**

Represents a key for template instantiation cache lookup. It includes:
- A string representation of the template name and arguments
- The template name
- The template kind (class, function, or variable)

**SerializedTemplateInstantiation**

A simple class that holds the serialized data of a template instantiation.

**CrossTranslationUnitContext**

Manages the template cache and provides an interface for the rest of Clang to interact with it. It:
- Initializes the template cache
- Configures the cache based on frontend options
- Provides methods to cache and look up template instantiations

Integration with Clang
---------------------

The template caching system is integrated with Clang's template instantiation process in `SemaTemplateInstantiate.cpp`. Before instantiating a template, Clang checks if the instantiation is already in the cache. If found, it uses the cached instantiation instead of performing the instantiation again. After successful instantiation, Clang stores the instantiation in the cache for future use.

The system is initialized in the `CrossTranslationUnitContext` constructor and configured based on frontend options:

.. code-block:: cpp

   CrossTranslationUnitContext::CrossTranslationUnitContext(CompilerInstance &CI)
       : Context(CI.getASTContext()),
         TemplateCache(std::make_unique<TemplateInstantiationCache>(CI.getASTContext())),
         ASTStorage(CI) {
     // Initialize template cache from frontend options
     if (CI.getFrontendOpts().TemplateCachingEnabled) {
       TemplateCache->setEnabled(true);
       if (!CI.getFrontendOpts().TemplateCacheDirectory.empty())
         TemplateCache->setCacheDirectory(CI.getFrontendOpts().TemplateCacheDirectory);
       if (!CI.getFrontendOpts().TemplateCachePrefix.empty())
         TemplateCache->setCacheFilePrefix(CI.getFrontendOpts().TemplateCachePrefix);
     }
   }

Caching and Serialization Mechanisms
-----------------------------------

The template caching system uses Clang's serialization infrastructure to serialize and deserialize template instantiations:

1. **Serialization**: When a template is instantiated, the resulting declaration is serialized using `ASTWriter` and stored in the cache.

2. **Deserialization**: When a cached template instantiation is needed, it is deserialized using `ASTReader` and imported into the current AST.

3. **Cache Storage**: The cache has two layers:
   - An in-memory cache for fast lookups during a single compilation session
   - A disk-based cache for persistence across compilation sessions

The cache files are stored in the specified cache directory with names based on the template key and the specified prefix.

Guidelines for Maintaining and Extending the Code
-----------------------------------------------

When working with or extending the template caching system, consider the following guidelines:

1. **Error Handling**: The system uses `llvm::Error` for error handling. Always check for errors when calling functions that return `llvm::Error` or `llvm::Expected<T>`.

2. **Thread Safety**: The current implementation is not thread-safe. If adding thread safety, ensure proper synchronization for both in-memory and disk-based caches.

3. **Cache Invalidation**: Consider adding mechanisms to invalidate cache entries when the corresponding template definitions change.

4. **Performance Considerations**: Serialization and deserialization can be expensive. Profile the code to ensure that the caching overhead doesn't outweigh the benefits.

5. **Testing**: Add tests for new functionality, especially for edge cases like template instantiations with complex arguments or nested templates.

Performance Documentation
=======================

Expected Performance Improvements
-------------------------------

The template caching system can provide significant performance improvements, especially for template-heavy codebases:

* **Compilation Time**: Reductions of 10-30% in total compilation time are common for codebases with heavy template usage.
* **Memory Usage**: Reduced memory usage due to sharing template instantiations across translation units.
* **Incremental Builds**: Faster incremental builds when changes don't affect template code.

The actual performance improvement depends on several factors, including the codebase's template usage patterns, the number of translation units, and the hardware specifications.

Factors Affecting Caching Efficiency
----------------------------------

Several factors can affect the efficiency of the template caching system:

1. **Template Complexity**: Complex templates with many parameters or nested templates may have larger serialization overhead.

2. **Template Usage Patterns**: Codebases that instantiate the same templates with the same arguments across multiple translation units benefit more from caching.

3. **Cache Hit Rate**: Higher cache hit rates lead to better performance. This depends on how frequently the same templates are instantiated with the same arguments.

4. **Disk I/O Performance**: Since the cache is stored on disk, the performance of the storage system can impact the overall efficiency.

5. **Serialization/Deserialization Overhead**: The time spent serializing and deserializing template instantiations can offset some of the benefits of caching.

Recommendations for Optimizing Template-Heavy Codebases
-----------------------------------------------------

To maximize the benefits of template caching:

1. **Reduce Template Instantiation Depth**: Deeply nested template instantiations can be expensive to serialize and deserialize. Consider refactoring to reduce nesting where possible.

2. **Use Explicit Template Instantiations**: For frequently used template instantiations, consider using explicit instantiations to reduce the number of implicit instantiations.

3. **Precompiled Headers**: Combine template caching with precompiled headers for maximum compilation speed.

4. **Optimize Include Hierarchy**: Minimize unnecessary includes to reduce the number of template instantiations.

5. **Monitor Cache Performance**: Use compilation time metrics to monitor the effectiveness of template caching and adjust your codebase accordingly.

Future Enhancements
==================

Potential future enhancements to the template caching system include:

1. **Improved Serialization**: Optimizing the serialization format to reduce the size of cached instantiations.

2. **Distributed Caching**: Support for distributed cache servers to share template instantiations across multiple machines.

3. **Smarter Cache Invalidation**: More sophisticated cache invalidation strategies based on template definition changes.

4. **Integration with Build Systems**: Better integration with build systems like CMake to manage the template cache more effectively.

5. **Performance Metrics**: Built-in performance metrics to measure cache hit rates and compilation time savings.

Conclusion
=========

The template caching system in Clang provides a powerful mechanism to reduce compilation time for template-heavy codebases. By caching and reusing template instantiations, it can significantly improve build times, especially for large C++ projects that make extensive use of templates.

Both users and developers can benefit from understanding how the system works and how to configure it for optimal performance. As the system evolves, it will continue to provide even greater benefits for C++ development workflows.