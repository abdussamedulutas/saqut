<?php
    if (!function_exists('public_path')) {
    /**
        * Get the path to the public folder.
        *
        * @param  string $path
        * @return string
        */
        function public_path($path = '')
        {
            return env('PUBLIC_PATH', base_path('public')) . ($path ? '/' . $path : $path);
        }
    }